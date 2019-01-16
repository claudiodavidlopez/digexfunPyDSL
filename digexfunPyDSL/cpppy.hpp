#pragma once

#include <Python.h>

#define MODULE_NAME            "script"
#define CALLABLE_REGISTRY_NAME "CALLABLE_REGISTRY"
#define MAX_CALLABLES          64


//------------------------------------------------------------------------------
// For loading a Python script and accessing its callables
//------------------------------------------------------------------------------
class CppPy
{
private:
    PyObject *pModule;
    PyObject *pCallableReg;

    // For debugging
    void (*Print)(const char *, unsigned int, ...);

public:
    CppPy(void);
    ~CppPy(void);
    unsigned int Load(void);
    PyObject *GetCallable(unsigned int callableID);

    // For debugging
    void Set_PrintFun(void (*PrintFun)(const char *, unsigned int,  ...));
};


//------------------------------------------------------------------------------
// For interacting with a Python callable (function or method)
//------------------------------------------------------------------------------
class PyCallable
{
private:
    unsigned int  argNum;
    unsigned int  retValNum;
    unsigned int  callableID;
    PyObject     *pArgTuple;
    PyObject     *pRetTuple;
    PyObject     *pCallable;

    // For debugging
    void (*Print)(const char *, unsigned int, ...);

public:
    PyCallable(unsigned int argNum, unsigned int callableID, CppPy *pCppPy);
    ~PyCallable(void);
    unsigned int SetArg(double argVal, unsigned int argID);
    unsigned int Call(void);
    unsigned int GetRetVal(double *pRetVal, unsigned int retValID);

    // For debugging
    void Set_PrintFun(void (*PrintFun)(const char *, unsigned int,  ...));
};
