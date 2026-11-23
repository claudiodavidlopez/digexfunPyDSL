#pragma once

#include <Python.h>

#include <string>

#define MODULE_NAME            "script"
#define CALLABLE_REGISTRY_NAME "CALLABLE_REGISTRY"
#define MAX_CALLABLES          64

// Error codes
enum {
    CPPPY_OK                  = 0,
    CPPPY_IMPORT_FAILED       = 1,
    CPPPY_NO_REGISTRY         = 2,
    CPPPY_REGISTRY_NOT_SEQ    = 3,
    CPPPY_INDEX_OUT_OF_RANGE  = 4,
    CPPPY_ARG_NOT_SET         = 5,
    CPPPY_CALL_FAILED         = 6,
    CPPPY_NOT_CALLED_YET      = 7,
    CPPPY_NOT_A_TUPLE         = 8,
    CPPPY_NOT_A_FLOAT         = 9,
    CPPPY_NO_MEMORY           = 10,
};

// Describe and clear the pending Python exception, including its traceback.
std::string FetchPythonError(void);


//------------------------------------------------------------------------------
// For loading a Python script and accessing its callables
//------------------------------------------------------------------------------
class CppPy
{
private:
    PyObject *pModule;
    PyObject *pCallableReg;

public:
    CppPy(void);
    ~CppPy(void);
    unsigned int Load(void);
    unsigned int RegistrySize(void);
    // Returns a new reference, or NULL with no Python error set.
    PyObject *GetCallable(unsigned int callableID);
};


//------------------------------------------------------------------------------
// For interacting with a Python callable (function or method)
//------------------------------------------------------------------------------
class PyCallable
{
private:
    unsigned int  argNum;
    unsigned int  retValNum;
    PyObject     *pArgTuple;
    PyObject     *pRetTuple;
    PyObject     *pCallable;

public:
    // Takes ownership of pCallable (a new reference).
    PyCallable(unsigned int argNum, PyObject *pCallable);
    ~PyCallable(void);
    unsigned int ArgCount(void) const { return argNum; }
    unsigned int RetValCount(void) const { return retValNum; }
    unsigned int SetArg(double argVal, unsigned int argID);
    unsigned int Call(unsigned int *pUnsetArg);
    unsigned int GetRetVal(double *pRetVal, unsigned int retValID);
};
