#include "cpppy.hpp"


//------------------------------------------------------------------------------
// Python error reporting
//------------------------------------------------------------------------------
std::string FetchPythonError(void)
{
    if (!PyErr_Occurred())
        return "";

    PyObject *pType, *pValue, *pTraceback;
    PyErr_Fetch(&pType, &pValue, &pTraceback);
    PyErr_NormalizeException(&pType, &pValue, &pTraceback);

    std::string text;
    PyObject *pTbModule = PyImport_ImportModule("traceback");
    PyObject *pLines = pTbModule
        ? PyObject_CallMethod(pTbModule, "format_exception", "OOO",
                              pType,
                              pValue ? pValue : Py_None,
                              pTraceback ? pTraceback : Py_None)
        : NULL;
    PyObject *pSep = PyUnicode_FromString("");
    PyObject *pJoined = (pLines && pSep) ? PyUnicode_Join(pSep, pLines) : NULL;
    const char *pUtf8 = pJoined ? PyUnicode_AsUTF8AndSize(pJoined, NULL) : NULL;

    if (pUtf8)
        text = pUtf8;
    else {
        PyErr_Clear();
        text = "(unable to format the Python exception)";
    }

    Py_XDECREF(pJoined);
    Py_XDECREF(pSep);
    Py_XDECREF(pLines);
    Py_XDECREF(pTbModule);
    Py_XDECREF(pType);
    Py_XDECREF(pValue);
    Py_XDECREF(pTraceback);
    return text;
}


//------------------------------------------------------------------------------
// For loading a Python script and accessing its callables
//------------------------------------------------------------------------------
CppPy::CppPy(void)
{
    Py_Initialize();
    this->pModule = NULL;
    this->pCallableReg = NULL;
}

CppPy::~CppPy(void)
{
    Py_XDECREF(this->pModule);
    Py_XDECREF(this->pCallableReg);
    Py_Finalize();
}

unsigned int CppPy::Load(void)
{
    // Release the previous run's registry
    Py_CLEAR(this->pCallableReg);

    Py_CLEAR(this->pModule);

    // Import a fresh copy of the script for every run. importlib.reload would
    // re-execute it in the old module's namespace, so names removed from the
    // script (even CALLABLE_REGISTRY) would survive from the previous run.
    PyObject *pModules = PyImport_GetModuleDict(); // borrowed
    if (PyDict_GetItemString(pModules, MODULE_NAME) &&
        PyDict_DelItemString(pModules, MODULE_NAME) < 0)
        return CPPPY_IMPORT_FAILED;
    // The old namespace usually sits in reference cycles (functions refer to
    // their module globals). Collect it now so whatever the previous run
    // held (sockets, files, models) is released before the new run starts.
    PyObject *pGc = PyImport_ImportModule("gc");
    PyObject *pCollected = pGc ? PyObject_CallMethod(pGc, "collect", NULL) : NULL;
    Py_XDECREF(pCollected);
    Py_XDECREF(pGc);
    PyErr_Clear();
    // Directory listings are cached by the import system; pick up a script
    // that was created after the last attempt.
    PyObject *pImportlib = PyImport_ImportModule("importlib");
    PyObject *pIgnored = pImportlib
        ? PyObject_CallMethod(pImportlib, "invalidate_caches", NULL)
        : NULL;
    Py_XDECREF(pIgnored);
    Py_XDECREF(pImportlib);
    PyErr_Clear();

    // Python errors are left set for the caller.
    this->pModule = PyImport_ImportModule(MODULE_NAME);
    if (!this->pModule)
        return CPPPY_IMPORT_FAILED;
    // Load callable registry
    this->pCallableReg = PyObject_GetAttrString(pModule, CALLABLE_REGISTRY_NAME);
    // Couldn't load registry. Keep the module so a later reload re-reads the
    // same file (reload needs a module object).
    if (!this->pCallableReg)
        return CPPPY_NO_REGISTRY;
    if (!PySequence_Check(this->pCallableReg)) {
        Py_CLEAR(this->pCallableReg);
        return CPPPY_REGISTRY_NOT_SEQ;
    }

    return CPPPY_OK;
}

unsigned int CppPy::RegistrySize(void)
{
    if (!this->pCallableReg)
        return 0;
    Py_ssize_t size = PySequence_Size(this->pCallableReg);
    if (size < 0) {
        PyErr_Clear();
        return 0;
    }
    return (unsigned int)size;
}

PyObject *CppPy::GetCallable(unsigned int callableID)
{
    if (!this->pCallableReg || callableID >= this->RegistrySize())
        return NULL;
    // PySequence_GetItem returns a new reference, owned by the caller
    PyObject *pCallable = PySequence_GetItem(this->pCallableReg, callableID);
    if (!pCallable)
        PyErr_Clear();
    return pCallable;
}


//------------------------------------------------------------------------------
// For interacting with a Python callable (function or method)
//------------------------------------------------------------------------------
PyCallable::PyCallable(unsigned int argNum, PyObject *pCallable)
{
    this->argNum     = argNum;
    this->pArgTuple  = argNum > 0 ? PyTuple_New(argNum) : NULL;
    this->pCallable  = pCallable;
    this->called     = false;
}

PyCallable::~PyCallable(void)
{
    Py_XDECREF(this->pArgTuple);
    Py_XDECREF(this->pCallable);
}

unsigned int PyCallable::SetArg(double argVal, unsigned int argID)
{
    // Boundary check
    if (argID >= this->argNum)
        return CPPPY_INDEX_OUT_OF_RANGE;
    if (!this->pArgTuple)
        return CPPPY_NO_MEMORY;
    // Convert to Python float
    PyObject *pArgVal = PyFloat_FromDouble(argVal);
    if (!pArgVal)
        return CPPPY_NO_MEMORY;
    // Set argument in tuple (steals the reference, releases the old value)
    if (PyTuple_SetItem(this->pArgTuple, argID, pArgVal) < 0)
        return CPPPY_NO_MEMORY;

    return CPPPY_OK;
}

unsigned int PyCallable::Call(unsigned int *pIndex)
{
    // Every argument must have been set at least once; a NULL tuple item
    // would crash the interpreter.
    for (unsigned int i = 0; i < this->argNum; i++) {
        if (!PyTuple_GetItem(this->pArgTuple, i)) {
            PyErr_Clear();
            *pIndex = i;
            return CPPPY_ARG_NOT_SET;
        }
    }
    // Forget the previous step's return values
    this->called = true;
    this->retVals.clear();
    // Call function or method
    PyObject *pResult = PyObject_CallObject(this->pCallable, this->pArgTuple);
    // Call failed (Python error is left set for the caller)
    if (!pResult)
        return CPPPY_CALL_FAILED;
    unsigned int error = this->StoreRetVals(pResult, pIndex);
    Py_DECREF(pResult);
    return error;
}

// float() also parses strings, but a string returned to a simulation model
// is a bug rather than a number.
static PyObject *ToPythonFloat(PyObject *pValue)
{
    if (PyUnicode_Check(pValue) || PyBytes_Check(pValue)) {
        PyErr_SetString(PyExc_TypeError, "expected a number, got a string");
        return NULL;
    }
    return PyNumber_Float(pValue);
}

// Accept a tuple or other sequence of numbers, a single number, or None.
// A "number" is anything float() accepts except strings. Python errors are
// left set.
unsigned int PyCallable::StoreRetVals(PyObject *pResult, unsigned int *pBadIndex)
{
    if (pResult == Py_None)
        return CPPPY_OK;

    bool isSequence = PySequence_Check(pResult) &&
                      !PyUnicode_Check(pResult) && !PyBytes_Check(pResult);
    Py_ssize_t count = isSequence ? PySequence_Size(pResult) : 1;
    if (count < 0) {
        *pBadIndex = 0;
        return CPPPY_NOT_A_NUMBER;
    }

    std::vector<double> values;
    values.reserve((size_t)count);
    for (Py_ssize_t i = 0; i < count; i++) {
        PyObject *pItem = isSequence ? PySequence_GetItem(pResult, i) : pResult;
        PyObject *pFloat = pItem ? ToPythonFloat(pItem) : NULL;
        if (isSequence)
            Py_XDECREF(pItem);
        if (!pFloat) {
            *pBadIndex = (unsigned int)i;
            return CPPPY_NOT_A_NUMBER;
        }
        values.push_back(PyFloat_AsDouble(pFloat));
        Py_DECREF(pFloat);
    }
    this->retVals.swap(values);
    return CPPPY_OK;
}

unsigned int PyCallable::GetRetVal(double *pRetVal, unsigned int retValID)
{
    if (!this->called)
        return CPPPY_NOT_CALLED_YET;
    if (retValID >= this->retVals.size())
        return CPPPY_INDEX_OUT_OF_RANGE;
    *pRetVal = this->retVals[retValID];
    return CPPPY_OK;
}
