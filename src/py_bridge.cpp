#include "py_bridge.hpp"

#include <format>
#include <mutex>

namespace digexfun {

// ---- errors ----------------------------------------------------------------

namespace {

// Format the pending Python exception like the interpreter would, using
// traceback.format_exception, and clear it.
std::string fetch_python_error()
{
    if (!PyErr_Occurred())
        return "(no Python exception was set)";

    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);
    PyRef type_ref  = PyRef::steal(type);
    PyRef value_ref = PyRef::steal(value);
    PyRef tb_ref    = PyRef::steal(traceback);

    PyRef module = PyRef::steal(PyImport_ImportModule("traceback"));
    PyRef lines  = module ? PyRef::steal(PyObject_CallMethod(
                                module.get(), "format_exception", "OOO", type,
                                value ? value : Py_None, traceback ? traceback : Py_None))
                          : PyRef();
    PyRef empty  = PyRef::steal(PyUnicode_FromString(""));
    PyRef text   = lines && empty ? PyRef::steal(PyUnicode_Join(empty.get(), lines.get()))
                                  : PyRef();
    const char *utf8 = text ? PyUnicode_AsUTF8AndSize(text.get(), nullptr) : nullptr;
    if (!utf8) {
        PyErr_Clear();
        return "(the Python exception could not be formatted)";
    }
    return utf8;
}

// Call module.function() for its side effect, ignoring failures.
void call_quietly(const char *module, const char *function)
{
    PyRef mod    = PyRef::steal(PyImport_ImportModule(module));
    PyRef result = mod ? PyRef::steal(PyObject_CallMethod(mod.get(), function, nullptr))
                       : PyRef();
    PyErr_Clear();
}

bool is_text(PyObject *object)
{
    return PyUnicode_Check(object) || PyBytes_Check(object);
}

} // namespace

void throw_python_error(const std::string &what)
{
    throw Error(what + ":\n" + fetch_python_error());
}

// ---- interpreter and GIL ---------------------------------------------------

GilLock::GilLock()
{
    // Started on first use rather than while the DLL is being loaded:
    // initialising the interpreter under the Windows loader lock is unsafe.
    // Py_InitializeEx(0) leaves PowerFactory's signal handlers alone. The
    // GIL is released right away so any thread can take it below.
    //
    // Python is never finalised. PowerFactory unloads the DLL when it
    // exits, which is too late to finalise safely, and many extension
    // modules (numpy among them) do not support re-initialisation.
    static std::once_flag started;
    std::call_once(started, [] {
        Py_InitializeEx(0);
        PyEval_SaveThread();
    });
    state_ = PyGILState_Ensure();
}

GilLock::~GilLock()
{
    PyGILState_Release(state_);
}

// ---- Script ----------------------------------------------------------------

void Script::load()
{
    registry_.reset();
    module_.reset();

    // A fresh import for every run. importlib.reload would re-execute the
    // script in the old module's namespace, so names removed from the
    // script (even CALLABLE_REGISTRY) would survive from the previous run.
    PyObject *modules = PyImport_GetModuleDict(); // borrowed
    if (PyDict_GetItemString(modules, module_name) &&
        PyDict_DelItemString(modules, module_name) < 0)
        throw_python_error("could not discard the previous script.py");

    // The old namespace usually sits in reference cycles (functions refer to
    // their module's globals). Collect it now, so whatever the previous run
    // held (sockets, files, models) is released before the new run starts.
    call_quietly("gc", "collect");
    // Directory listings are cached by the import system; pick up a script
    // created after the last attempt.
    call_quietly("importlib", "invalidate_caches");

    PyRef module = PyRef::steal(PyImport_ImportModule(module_name));
    if (!module)
        throw_python_error("could not import script.py");

    PyRef registry = PyRef::steal(PyObject_GetAttrString(module.get(), registry_name));
    if (!registry) {
        PyErr_Clear();
        throw Error("script.py does not define CALLABLE_REGISTRY");
    }
    if (!PySequence_Check(registry.get()) || is_text(registry.get()))
        throw Error("CALLABLE_REGISTRY in script.py must be a list of callables");

    module_   = std::move(module);
    registry_ = std::move(registry);
}

std::size_t Script::size() const
{
    if (!registry_)
        return 0;
    const Py_ssize_t size = PySequence_Size(registry_.get());
    if (size < 0) {
        PyErr_Clear();
        return 0;
    }
    return static_cast<std::size_t>(size);
}

PyRef Script::callable(std::size_t index) const
{
    PyRef item = PyRef::steal(
        PySequence_GetItem(registry_.get(), static_cast<Py_ssize_t>(index)));
    if (!item)
        throw_python_error(std::format("could not read CALLABLE_REGISTRY[{}]", index));
    if (!PyCallable_Check(item.get()))
        throw Error(std::format("CALLABLE_REGISTRY[{}] is not callable", index));
    return item;
}

// ---- Callable --------------------------------------------------------------

Callable::Callable(PyRef function, std::size_t registry_index, std::size_t arg_count)
    : function_(std::move(function)),
      registry_index_(registry_index),
      args_(arg_count, 0.0),
      arg_set_(arg_count, false)
{
}

void Callable::set_arg(std::size_t index, double value)
{
    args_.at(index)    = value;
    arg_set_.at(index) = true;
}

void Callable::call()
{
    for (std::size_t i = 0; i < args_.size(); ++i)
        if (!arg_set_[i])
            throw Error(std::format("argument {} was never set with SetPyFunArg", i));

    called_ = true;
    results_.clear();

    PyRef args = PyRef::steal(PyTuple_New(static_cast<Py_ssize_t>(args_.size())));
    if (!args)
        throw_python_error("could not build the argument tuple");
    for (std::size_t i = 0; i < args_.size(); ++i) {
        PyObject *value = PyFloat_FromDouble(args_[i]);
        if (!value)
            throw_python_error("could not build the argument tuple");
        PyTuple_SetItem(args.get(), static_cast<Py_ssize_t>(i), value); // steals value
    }

    PyRef result = PyRef::steal(PyObject_CallObject(function_.get(), args.get()));
    if (!result)
        throw_python_error(
            std::format("CALLABLE_REGISTRY[{}] raised an exception", registry_index_));

    results_ = to_numbers(result.get());
}

// Accept a tuple, list or other sequence of numbers, a single number, or
// None (no values). A number is anything float() accepts, except strings:
// float() parses those, but a string returned to a model is a bug.
std::vector<double> Callable::to_numbers(PyObject *result) const
{
    if (result == Py_None)
        return {};

    const bool       is_sequence = PySequence_Check(result) && !is_text(result);
    const Py_ssize_t count       = is_sequence ? PySequence_Size(result) : 1;

    std::vector<double> numbers;
    for (Py_ssize_t i = 0; count >= 0 && i < count; ++i) {
        PyRef item  = is_sequence ? PyRef::steal(PySequence_GetItem(result, i))
                                  : PyRef::borrow(result);
        PyRef as_float;
        if (item && is_text(item.get()))
            PyErr_SetString(PyExc_TypeError, "expected a number, got a string");
        else if (item)
            as_float = PyRef::steal(PyNumber_Float(item.get()));

        if (!as_float)
            throw_python_error(std::format("return value {} of CALLABLE_REGISTRY[{}] is not a number",
                                           i, registry_index_));
        numbers.push_back(PyFloat_AsDouble(as_float.get()));
    }
    if (count < 0)
        throw_python_error(std::format("CALLABLE_REGISTRY[{}] returned an unreadable sequence",
                                       registry_index_));
    return numbers;
}

} // namespace digexfun
