#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

#include "digexfun.hpp"
#include "cpppy.hpp"


//------------------------------------------------------------------------------
// Standard PowerFactory functions for custom DLL creation.
// Only RegisterFunctions needs to be modified.
//
// Remember to add new functions to the digexfun.def file to make them
// available to PowerFactory.
//------------------------------------------------------------------------------

// pointer to print function (set on Init() call when DLL is loaded)
FP_PRINTMSG pPrintFnc(NULL);
FP_STOPSIM  pStopSimFnc(NULL);

// this method is called when loading dll into powerfactory
void __stdcall Init(FP_PRINTMSG _pPrintFnc, FP_STOPSIM _pStopSimFnc)
{
    pPrintFnc   = _pPrintFnc;
    pStopSimFnc = _pStopSimFnc;
}

// print a message to PowerFactory's output window
void print_pf(const char *msg, unsigned int msgType)
{
    if (pPrintFnc) {
        (*pPrintFnc)(msg, msgType);
    }
}

void stop_simulation()
{
    if (pStopSimFnc) {
        (pStopSimFnc)();
    }
}

int __stdcall RegisterFunctions(int ifun, char *cnam, int *iargc)
{
    // register userdefined functions number ifun:
    //     return name (cnam[50]) and number of arguments (iargc)
    //     return != 0, if ifun exceeds the number of available functions
    if (!cnam || !iargc) {
        return 1;
    }

    LoadDigsiLibrary();

    if (ifun == 0) {
        strcpy(cnam, "LoadPyFun");
        *iargc = 2;
    } else if (ifun == 1) {
        strcpy(cnam, "SetPyFunArg");
        *iargc = 3;
    } else if (ifun == 2) {
        strcpy(cnam, "CallPyFun");
        *iargc = 1;
    } else if (ifun == 3) {
        strcpy(cnam, "GetPyFunRetVal");
        *iargc = 2;
    } else {
        // unknown function
        return 1;
    }

    return 0;
}

//------------------------------------------------------------------------------
// Error reporting
//------------------------------------------------------------------------------

// Print an error to the PowerFactory output window, one message per line,
// and ask PowerFactory to stop the simulation.
static void ReportError(const std::string &text)
{
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
        if (!line.empty())
            print_pf(line.c_str(), MSG_ERR);
    stop_simulation();
}

static void ReportErrorf(const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    ReportError(buffer);
}

//------------------------------------------------------------------------------
// Interface between DSL and C++
//------------------------------------------------------------------------------
#define MAX_ARGUMENTS 1024

static CppPy        gCppPy                      = CppPy();
static PyCallable  *gPyCallable[MAX_CALLABLES]  = {};
static unsigned int gCallCounter[MAX_CALLABLES] = {0};
static bool         gInitialized                = false;


void FreeCallables(void)
{
    for (unsigned int i=0; i<MAX_CALLABLES; i++) {
            delete gPyCallable[i];
            gPyCallable[i] = NULL;
    }
}


unsigned int Max(unsigned int *array, unsigned int array_size)
{
    unsigned int max = 0;

    for (unsigned int i = 0; i<array_size; i++)
        if (array[i] > max)
            max = array[i];

    return max;
}


void ResetCallCounter(void)
{
    for (int i = 0; i<MAX_CALLABLES; i++)
        gCallCounter[i] = 0;
}


// DSL passes everything as doubles. Accept only whole numbers in [0, limit).
static bool ToIndex(double value, unsigned int limit, unsigned int *pIndex)
{
    if (!(value >= 0.0) || value >= (double)limit || value != std::floor(value))
        return false;
    *pIndex = (unsigned int)value;
    return true;
}


// Validate a function ID passed from DSL and return its callable, or NULL
// after reporting the problem.
static PyCallable *LoadedCallable(const char *dslFunction, double idVal)
{
    unsigned int pyFunID;
    if (!ToIndex(idVal, MAX_CALLABLES, &pyFunID)) {
        ReportErrorf("%s: function ID %g is not a whole number in [0, %d)",
                     dslFunction, idVal, MAX_CALLABLES);
        return NULL;
    }
    if (!gPyCallable[pyFunID]) {
        ReportErrorf("%s: function ID %u was not loaded; call LoadPyFun(%u, ...) "
                     "in the model's initialisation first",
                     dslFunction, pyFunID, pyFunID);
        return NULL;
    }
    return gPyCallable[pyFunID];
}


void __stdcall LoadPyFun(void)
{
    // Get arguments
    double argNumVal  = pop();
    double pyFunIDVal = pop();
    // Every DSL function returns a value
    push(0);

    unsigned int pyFunID, argNum;
    if (!ToIndex(pyFunIDVal, MAX_CALLABLES, &pyFunID)) {
        ReportErrorf("LoadPyFun: function ID %g is not a whole number in [0, %d)",
                     pyFunIDVal, MAX_CALLABLES);
        return;
    }
    if (!ToIndex(argNumVal, MAX_ARGUMENTS, &argNum)) {
        ReportErrorf("LoadPyFun: argument count %g is not a whole number in [0, %d)",
                     argNumVal, MAX_ARGUMENTS);
        return;
    }

    if (!gInitialized) { // Code executed only once per simulation run
        FreeCallables();
        ResetCallCounter();
        switch (gCppPy.Load()) {
        case CPPPY_OK:
            break;
        case CPPPY_IMPORT_FAILED:
            ReportError("LoadPyFun: could not import " MODULE_NAME ".py:\n" +
                        FetchPythonError());
            return;
        case CPPPY_NO_REGISTRY:
            PyErr_Clear();
            ReportError("LoadPyFun: " MODULE_NAME ".py does not define "
                        CALLABLE_REGISTRY_NAME);
            return;
        default:
            ReportError("LoadPyFun: " CALLABLE_REGISTRY_NAME " in " MODULE_NAME
                        ".py must be a list of callables");
            return;
        }
        gInitialized = true;
    }

    if (gPyCallable[pyFunID]) {
        ReportErrorf("LoadPyFun: function ID %u was loaded more than once", pyFunID);
        return;
    }

    unsigned int registrySize = gCppPy.RegistrySize();
    if (pyFunID >= registrySize) {
        ReportErrorf("LoadPyFun: function ID %u is out of range; "
                     CALLABLE_REGISTRY_NAME " has %u entries", pyFunID, registrySize);
        return;
    }

    PyObject *pCallable = gCppPy.GetCallable(pyFunID);
    if (!pCallable || !PyCallable_Check(pCallable)) {
        Py_XDECREF(pCallable);
        ReportErrorf("LoadPyFun: " CALLABLE_REGISTRY_NAME "[%u] is not callable",
                     pyFunID);
        return;
    }

    gPyCallable[pyFunID] = new PyCallable(argNum, pCallable);
}


void __stdcall SetPyFunArg(void)
{
    // Get arguments
    double argVal     = pop();
    double argIDVal   = pop();
    double pyFunIDVal = pop();
    push(0);

    PyCallable *pFun = LoadedCallable("SetPyFunArg", pyFunIDVal);
    if (!pFun)
        return;

    unsigned int argID;
    if (!ToIndex(argIDVal, pFun->ArgCount(), &argID)) {
        ReportErrorf("SetPyFunArg: argument index %g is out of range; function ID "
                     "%g was loaded with %u arguments",
                     argIDVal, pyFunIDVal, pFun->ArgCount());
        return;
    }
    if (pFun->SetArg(argVal, argID) != CPPPY_OK)
        ReportError("SetPyFunArg: out of memory:\n" + FetchPythonError());
}


void __stdcall CallPyFun(void)
{
    // Get arguments
    double pyFunIDVal = pop();
    push(0);

    PyCallable *pFun = LoadedCallable("CallPyFun", pyFunIDVal);
    if (!pFun)
        return;
    unsigned int pyFunID = (unsigned int)pyFunIDVal;

    gCallCounter[pyFunID]++;

    if (Max(gCallCounter, MAX_CALLABLES) > 5)
        gInitialized = false;

    unsigned int unsetArg = 0;
    switch (pFun->Call(&unsetArg)) {
    case CPPPY_OK:
        break;
    case CPPPY_ARG_NOT_SET:
        ReportErrorf("CallPyFun: function ID %u: argument %u was never set with "
                     "SetPyFunArg", pyFunID, unsetArg);
        break;
    case CPPPY_NOT_A_TUPLE:
        ReportErrorf("CallPyFun: " CALLABLE_REGISTRY_NAME "[%u] must return a tuple",
                     pyFunID);
        break;
    default: {
        char header[128];
        snprintf(header, sizeof(header),
                 "CallPyFun: " CALLABLE_REGISTRY_NAME "[%u] raised an exception:\n",
                 pyFunID);
        ReportError(header + FetchPythonError());
        break;
    }
    }
}


void __stdcall GetPyFunRetVal(void)
{
    // Get arguments
    double retValIDVal = pop();
    double pyFunIDVal  = pop();

    double retVal = 0.0;
    PyCallable *pFun = LoadedCallable("GetPyFunRetVal", pyFunIDVal);
    if (pFun) {
        unsigned int retValID = 0;
        unsigned int error = ToIndex(retValIDVal, MAX_ARGUMENTS, &retValID)
                                 ? pFun->GetRetVal(&retVal, retValID)
                                 : CPPPY_INDEX_OUT_OF_RANGE;
        switch (error) {
        case CPPPY_OK:
            break;
        case CPPPY_NOT_CALLED_YET:
            ReportErrorf("GetPyFunRetVal: function ID %g has no return values "
                         "before CallPyFun is called", pyFunIDVal);
            break;
        case CPPPY_INDEX_OUT_OF_RANGE:
            ReportErrorf("GetPyFunRetVal: return value index %g is out of range; "
                         "function ID %g returned %u values",
                         retValIDVal, pyFunIDVal, pFun->RetValCount());
            break;
        default:
            ReportErrorf("GetPyFunRetVal: return value %u of function ID %g is not "
                         "a float", retValID, pyFunIDVal);
            break;
        }
        if (error != CPPPY_OK)
            retVal = 0.0;
    }

    // Set return value
    push(retVal);
}
