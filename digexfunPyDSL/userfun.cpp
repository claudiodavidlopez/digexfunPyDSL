#include <sstream>
#include <stdio.h>
#include <stdarg.h>
#include "digexfun.hpp"
#include "cpppy.hpp"


using namespace std;


//------------------------------------------------------------------------------
// Standard PowerFactory functions for custom DLL creitoan.
// Only RegisterFunctions needs to be modified.
//
// Remember to add new functions to the degexfun.def file to make them available
// to PowerFactory.
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
// Print formated to PowerFactory terminal
//------------------------------------------------------------------------------
void printf_pf(const char *msg, unsigned int msgType, ...)
{
    va_list pArg;
    char fMsg[1024];
    va_start(pArg, msgType);
    vsprintf(fMsg, msg, pArg);
    print_pf(fMsg, msgType);
    va_end(pArg);
}

//------------------------------------------------------------------------------
// Interface between DSL and C++
//------------------------------------------------------------------------------
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


void __stdcall LoadPyFun(void)
{
    // Get arguments
    unsigned int argNum  = (unsigned int)pop();
    unsigned int pyFunID = (unsigned int)pop();

    // Function body
    if (!gInitialized) { // Code executed only once per simulation run
        FreeCallables();
        ResetCallCounter();
        unsigned int error = gCppPy.Load();
        if (error) {
            printf_pf("Failed to load module, raised error %u", MSG_ERR, error);
            stop_simulation();
        }
        gInitialized = true;
    }

    if (gPyCallable[pyFunID]) {
        printf_pf("Multiple attempts to load function %u", MSG_ERR, pyFunID);
        stop_simulation();
    }

    gPyCallable[pyFunID] = new PyCallable(argNum, pyFunID, &gCppPy);
    gPyCallable[pyFunID]->Set_PrintFun(printf_pf);

    // Set return value
    push(0);
}


void __stdcall SetPyFunArg(void)
{
    // Get arguments
    double       argVal  =               pop();
    unsigned int argID   = (unsigned int)pop();
    unsigned int pyFunID = (unsigned int)pop();

    // Function body
    unsigned int error = gPyCallable[pyFunID]->SetArg(argVal, argID);
    if (error) {
        printf_pf("SetPyFunArg (ID %u) raised error %u", MSG_ERR, pyFunID, error);
        stop_simulation();
    }

    // Set return value
    push(0);
}


void __stdcall CallPyFun(void)
{
    // Get arguments
    unsigned int pyFunID = (unsigned int)pop();

    // Function body
    gCallCounter[pyFunID]++;

    if (Max(gCallCounter, MAX_CALLABLES) > 5)
        gInitialized = false;

    unsigned int error = gPyCallable[pyFunID]->Call();
    if (error) {
        printf_pf("CallPyFun (ID %u) raised error %u", MSG_ERR, pyFunID, error);
        stop_simulation();
    }

    // Set return value
    push(0);
}


void __stdcall GetPyFunRetVal(void)
{
    // Get arguments
    unsigned int retValID = (unsigned int)pop();
    unsigned int pyFunID  = (unsigned int)pop();

    // Function body
    double retVal;
    unsigned int error = gPyCallable[pyFunID]->GetRetVal(&retVal, retValID);
    if (error) {
        printf_pf("GetPyFunRetVal (ID %u) raised error %u", MSG_ERR, pyFunID, error);
        stop_simulation();
    }

    // Set return value
    push(retVal);
}
