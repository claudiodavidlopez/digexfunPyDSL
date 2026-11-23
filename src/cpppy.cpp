#include "cpppy.hpp"


//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------
const char *PyObject_AsString(PyObject *object)
{
    PyObject *repr = PyObject_Repr(object);
    if (!repr)
        return NULL;

    PyObject *str = PyUnicode_AsEncodedString(repr, "utf-8", "~E~");
    if (!str) {
        Py_XDECREF(repr);
        return NULL;
    }

    Py_XDECREF(repr);

    return PyBytes_AS_STRING(str);
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
    // Load module on first call
    if (!this->pModule) {
        PyObject *pModuleName = PyUnicode_DecodeFSDefault(MODULE_NAME);
        this->pModule = PyImport_Import(pModuleName);
        Py_DECREF(pModuleName);
    }
    // Reload module on second call and after
    else {
        this->pModule = PyImport_ReloadModule(this->pModule);
    }
    // Couldn't load module
    if (!this->pModule)
        return 1;
    // Load callable registry
    this->pCallableReg = PyObject_GetAttrString(pModule, CALLABLE_REGISTRY_NAME);
    // Couldn't load registry
    if (!this->pCallableReg) {
        Py_DECREF(this->pModule);
        return 2;
    }

    return 0;
}

PyObject *CppPy::GetCallable(unsigned int callableID)
{
    if (!this->pCallableReg)
        return NULL;

    return PyList_GetItem(this->pCallableReg, callableID);
}

void CppPy::Set_PrintFun(void (*PrintFun)(const char *, unsigned int,  ...))
{
    this->Print = PrintFun;
}


//------------------------------------------------------------------------------
// For interacting with a Python callable (function or method)
//------------------------------------------------------------------------------
PyCallable::PyCallable(unsigned int argNum, unsigned int callableID, CppPy *pCppPy)
{
    this->argNum     = argNum;
    this->callableID = callableID;
    this->pArgTuple  = NULL;
    this->pRetTuple  = NULL;
    this->pCallable  = pCppPy->GetCallable(callableID);
}

PyCallable::~PyCallable(void)
{
    Py_XDECREF(this->pArgTuple);
    Py_XDECREF(this->pRetTuple);
    Py_XDECREF(this->pCallable);
}

unsigned int PyCallable::SetArg(double argVal, unsigned int argID)
{
    // Boundary check
    if (argID >= this->argNum)
        return 1;
    // Create new argument tuple if there isn't one already
    if (!this->pArgTuple)
        pArgTuple = PyTuple_New(this->argNum);
    // Now there should be an argument tuple
    if (!this->pArgTuple)
        return 2;
    // Convert to Python float
    PyObject *pArgVal = PyFloat_FromDouble(argVal);
    // Couln't convert to float
    if (!pArgVal)
        return 3;
    // Set argument in tuple
    if (PyTuple_SetItem(this->pArgTuple, argID, pArgVal) < 0)
        return 4; // Couln't set argument

    return 0;
}

unsigned int PyCallable::Call(void)
{
    if (!this->pCallable)
        return 1;
    // Call function or method
    this->pRetTuple = PyObject_CallObject(this->pCallable, this->pArgTuple);
    // Call failed
    if (!pRetTuple)
        return 2;
    // Update number of returned values
    this->retValNum = PyTuple_Size(pRetTuple);

    return 0;
}

unsigned int PyCallable::GetRetVal(double *pRetVal, unsigned int retValID)
{
    // Boundary check
    if (retValID >= this->retValNum)
        return 1;
    // Get item from tuple
    PyObject *pTupleItem = PyTuple_GetItem(this->pRetTuple, retValID);
    // Item not found
    if (!pTupleItem)
        return 2;
    // Item not a float
    if (!PyFloat_Check(pTupleItem))
        return 3;
    // Put the requested item (returned value) in caller's variable
    *pRetVal = PyFloat_AsDouble(pTupleItem);

    return 0;
}

void PyCallable::Set_PrintFun(void (*PrintFun)(const char *, unsigned int,  ...))
{
    this->Print = PrintFun;
}
