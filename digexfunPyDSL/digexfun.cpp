/***********************************************************************
* digexfun.cpp: DLLMain function
***********************************************************************/

#include "stdio.h"
#include "stdlib.h"
#include <windows.h>

#include "digexfun.hpp"


typedef void (__cdecl * FP_PUSH)(double);
typedef double * (__cdecl * FP_POP)(void);
typedef const char * (__cdecl * FP_WRK)(void);
typedef void (__cdecl * FP_EMITMSG)(const char *);
typedef void (__stdcall * FP_EXFUN)(void);

static FP_POP  fpPop     = NULL; // function pointer to pop()
static FP_PUSH fpPush    = NULL; // function pointer to push()
static FP_WRK  fpWrk     = NULL; // function pointer to get_wrk_path()
static HMODULE hLib      = NULL; // digadm.dll
static FP_EMITMSG fpEmit = NULL;

void __cdecl emitMessage(const char *msg) {
    if (fpEmit) {
        (*fpEmit)(msg);
    }
}

double __stdcall pop(void)
{ // pop : pops and returns the top-most value from the double_stack
    if (fpPop) {
        return *(*fpPop)();
    }
    return 0.;
}

void __stdcall push(double f)
{ // push : pushes a double on the double-value stack
    if (fpPush) {
        (*fpPush)(f);
    }
}

const char * __stdcall get_wrk_path()
{ // get working directory
    if (fpWrk) {
        return (*fpWrk)();
    }
    return NULL;
}

void LoadDigsiLibrary()
{ // load library digadm.dll, if not already done
    if (hLib) return;

    if ((hLib = LoadLibrary("digadm.dll"))) {
        fpPop  = (FP_POP)GetProcAddress(hLib, "pop");
        fpPush = (FP_PUSH)GetProcAddress(hLib, "push");
        fpWrk  = (FP_WRK)GetProcAddress(hLib, "get_wrk_path");
        fpEmit = (FP_EMITMSG)GetProcAddress(hLib, "emitMessage");
    }
}

void FreeDigsiLibrary()
{ // free library digadm.dll
    if (hLib) {
        int iret = FreeLibrary((struct HINSTANCE__ *)hLib);
    }
    hLib = NULL;
}

BOOL WINAPI DllMain(HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        //
        // DLL is attaching to the address space of the current process.
        //

        // LoadDigsiLibrary(); will cause access violation later on

        // push(3.4); // test

        // char buf[BUFSIZE+1];
        // ghMod = hDLL;

        /*
           GetModuleFileName (NULL, (LPTSTR) buf, BUFSIZE);
           MessageBox ( GetFocus(),
                    (LPCTSTR) buf,
                    (LPCTSTR) "digexfun: Process attaching",
                    MB_OK | MB_SYSTEMMODAL);
         */
        break;
    }

    case DLL_THREAD_ATTACH:

        //
        // A new thread is being created in the current process.
        //

        /*
           MessageBox ( GetFocus(),
                    (LPCTSTR) "digexfun: Thread attaching",
                    (LPCTSTR) "",
                    MB_OK | MB_SYSTEMMODAL);
         */
        break;

    case DLL_THREAD_DETACH:

        //
        // A thread is exiting cleanly.
        //

        /*
           MessageBox ( GetFocus(),
                    (LPCTSTR) "digexfun: Thread detaching",
                    (LPCTSTR) "",
                    MB_OK | MB_SYSTEMMODAL);
         */
        break;

    case DLL_PROCESS_DETACH:

        FreeDigsiLibrary();

        //
        // The calling process is detaching the DLL from its address space.
        //

        /*
           MessageBox ( GetFocus(),
                    (LPCTSTR) "digexfun: Process detaching",
                    (LPCTSTR) "",
                    MB_OK | MB_SYSTEMMODAL );
         */
        break;
    }

    return TRUE;
}
