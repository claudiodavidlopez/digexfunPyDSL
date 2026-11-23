// The four functions DSL models call, and their registration with
// PowerFactory. See README.md for how they are used from DSL.
//
// Every entry point pops its arguments, pushes exactly one return value
// (0.0, or the requested return value), and never lets an exception or a
// crash reach PowerFactory: problems are printed to the output window and
// the simulation is stopped.

#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <optional>
#include <sstream>
#include <string>

#include "digexfun.hpp"
#include "py_bridge.hpp"

using digexfun::Callable;
using digexfun::Error;
using digexfun::GilLock;
using digexfun::Script;

namespace {

constexpr std::size_t kMaxFunctions = 64;
constexpr std::size_t kMaxArguments = 1024;

// State kept across calls and simulation runs.
//
// PowerFactory keeps the DLL loaded between runs and gives no signal when
// a new one starts. A run starts with the LoadPyFun calls in the DSL
// blocks' initialisation, so loading an ID that is already loaded means a
// new run (or a repeated initialisation) has started. An error also ends
// the run, since it stops the simulation.
struct Session {
    Script                                          script;
    bool                                            script_loaded = false;
    std::array<std::optional<Callable>, kMaxFunctions> functions;

    void start_run()
    {
        for (auto &function : functions)
            function.reset();
        script_loaded = false;
        script.load();
        script_loaded = true;
    }
};

// Never destroyed: it holds Python objects, and Python is never finalised.
Session &session()
{
    static Session *instance = new Session;
    return *instance;
}

// DSL passes everything as doubles; indices must be whole numbers >= 0.
std::size_t to_index(double value, const char *what)
{
    if (!(value >= 0.0) || value != std::floor(value) || value >= 1e9)
        throw Error(std::format("{} {} is not a valid index (a whole number >= 0)", what, value));
    return static_cast<std::size_t>(value);
}

std::size_t to_function_id(double value)
{
    const std::size_t id = to_index(value, "function ID");
    if (id >= kMaxFunctions)
        throw Error(std::format("function ID {} is out of range; IDs 0 to {} are supported",
                                id, kMaxFunctions - 1));
    return id;
}

Callable &loaded_function(double id_value)
{
    const std::size_t id = to_function_id(id_value);
    auto &function = session().functions[id];
    if (!function)
        throw Error(std::format("function ID {} was not loaded; call LoadPyFun({}, ...) "
                                "in the model's initialisation first",
                                id, id));
    return *function;
}

// Print each line of the message as an error and stop the simulation.
void report(const char *dsl_function, const std::string &message)
{
    std::istringstream lines(dsl_function + std::string(": ") + message);
    for (std::string line; std::getline(lines, line);)
        if (!line.empty())
            pf::print(line, pf::MessageType::Error);
    session().script_loaded = false;
    pf::stop_simulation();
}

// Run the body of a DSL function, turning any failure into a report.
template <typename Body>
double guarded(const char *dsl_function, Body body) noexcept
{
    try {
        return body();
    } catch (const Error &e) {
        report(dsl_function, e.what());
    } catch (const std::exception &e) {
        report(dsl_function, std::string("internal error: ") + e.what());
    } catch (...) {
        report(dsl_function, "unknown internal error");
    }
    return 0.0;
}

double load_function(double id_value, double arg_count_value)
{
    const std::size_t id        = to_function_id(id_value);
    const std::size_t arg_count = to_index(arg_count_value, "argument count");
    if (arg_count > kMaxArguments)
        throw Error(std::format("argument count {} is too large (at most {})",
                                arg_count, kMaxArguments));

    GilLock gil;
    Session &s = session();
    if (!s.script_loaded || s.functions[id])
        s.start_run();

    if (id >= s.script.size())
        throw Error(std::format("function ID {} is out of range; CALLABLE_REGISTRY has {} entries",
                                id, s.script.size()));
    s.functions[id].emplace(s.script.callable(id), id, arg_count);
    return 0.0;
}

double set_argument(double id_value, double arg_index_value, double value)
{
    Callable &function = loaded_function(id_value);
    const std::size_t arg_index = to_index(arg_index_value, "argument index");
    if (arg_index >= function.arg_count())
        throw Error(std::format("argument index {} is out of range; function ID {} was "
                                "loaded with {} arguments",
                                arg_index, id_value, function.arg_count()));
    function.set_arg(arg_index, value);
    return 0.0;
}

double call_function(double id_value)
{
    GilLock gil;
    loaded_function(id_value).call();
    return 0.0;
}

double return_value(double id_value, double ret_index_value)
{
    const Callable &function = loaded_function(id_value);
    if (!function.called())
        throw Error(std::format("function ID {} has no return values before CallPyFun "
                                "is called",
                                id_value));
    const std::size_t ret_index = to_index(ret_index_value, "return value index");
    if (ret_index >= function.results().size())
        throw Error(std::format("return value index {} is out of range; function ID {} "
                                "returned {} values",
                                ret_index, id_value, function.results().size()));
    return function.results()[ret_index];
}

struct DslFunction {
    const char *name;
    int         arg_count;
};

constexpr DslFunction kDslFunctions[] = {
    {"LoadPyFun", 2},      // LoadPyFun(function ID, number of arguments)
    {"SetPyFunArg", 3},    // SetPyFunArg(function ID, argument index, value)
    {"CallPyFun", 1},      // CallPyFun(function ID)
    {"GetPyFunRetVal", 2}, // GetPyFunRetVal(function ID, return value index)
};

} // namespace

// Called by PowerFactory with index 0, 1, ... until it returns non-zero.
// `name` points to a buffer of 50 characters.
extern "C" int __stdcall RegisterFunctions(int index, char *name, int *arg_count)
{
    if (!name || !arg_count || index < 0 || index >= static_cast<int>(std::size(kDslFunctions)))
        return 1;
    strcpy_s(name, 50, kDslFunctions[index].name);
    *arg_count = kDslFunctions[index].arg_count;
    return 0;
}

// DSL arguments are popped in reverse order.

extern "C" void __stdcall LoadPyFun()
{
    const double arg_count = pf::pop();
    const double id        = pf::pop();
    pf::push(guarded("LoadPyFun", [&] { return load_function(id, arg_count); }));
}

extern "C" void __stdcall SetPyFunArg()
{
    const double value     = pf::pop();
    const double arg_index = pf::pop();
    const double id        = pf::pop();
    pf::push(guarded("SetPyFunArg", [&] { return set_argument(id, arg_index, value); }));
}

extern "C" void __stdcall CallPyFun()
{
    const double id = pf::pop();
    pf::push(guarded("CallPyFun", [&] { return call_function(id); }));
}

extern "C" void __stdcall GetPyFunRetVal()
{
    const double ret_index = pf::pop();
    const double id        = pf::pop();
    pf::push(guarded("GetPyFunRetVal", [&] { return return_value(id, ret_index); }));
}
