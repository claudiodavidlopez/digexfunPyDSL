// The PowerFactory side of the DLL.
//
// PowerFactory talks to a digexfun DLL through two channels:
//  - Init() hands the DLL two callbacks: one prints to the output window,
//    the other asks PowerFactory to stop the running simulation.
//  - DSL function arguments and return values travel through a stack of
//    doubles that lives in PowerFactory's own digadm.dll, which exports
//    push() and pop().
// This file wraps both behind the small pf:: API in digexfun.hpp, so the
// rest of the code never deals with raw function pointers.

#include "digexfun.hpp"

#include <mutex>

#include <windows.h>

namespace {

// Signatures as declared in DIgSILENT's digexfun example. The calling
// conventions only matter for 32-bit builds: x64 has a single convention
// and the compiler ignores __cdecl and __stdcall there.
using PushFn  = void(__cdecl *)(double);
using PopFn   = double *(__cdecl *)(void); // returns a pointer to the popped value
using PrintFn = void(__stdcall *)(const char *, unsigned int);
using StopFn  = void(__stdcall *)(void);

PushFn  g_push  = nullptr;
PopFn   g_pop   = nullptr;
PrintFn g_print = nullptr; // set by Init
StopFn  g_stop  = nullptr; // set by Init

// digadm.dll belongs to PowerFactory and is already loaded in the process,
// so LoadLibrary just returns its handle. It is resolved on first use rather
// than in DllMain, where loading libraries is unsafe (the loader lock is
// held). The reference is never released; PowerFactory keeps the library
// loaded for its whole lifetime anyway.
void resolve_digadm()
{
    static std::once_flag resolved;
    std::call_once(resolved, [] {
        if (HMODULE digadm = LoadLibraryA("digadm.dll")) {
            g_push = reinterpret_cast<PushFn>(GetProcAddress(digadm, "push"));
            g_pop  = reinterpret_cast<PopFn>(GetProcAddress(digadm, "pop"));
        }
    });
}

} // namespace

// Called by PowerFactory once, right after it loads the DLL at start-up.
// Either callback may be null; printing and stopping are then no-ops.
extern "C" void __stdcall Init(PrintFn print, StopFn stop)
{
    g_print = print;
    g_stop  = stop;
}

namespace pf {

// Without digadm.dll (i.e. outside PowerFactory) there is no stack: pop()
// yields 0.0 and push() discards the value, rather than crashing the host.

double pop()
{
    resolve_digadm();
    return g_pop ? *g_pop() : 0.0;
}

void push(double value)
{
    resolve_digadm();
    if (g_push)
        g_push(value);
}

void print(const std::string &message, MessageType type)
{
    if (g_print)
        g_print(message.c_str(), static_cast<unsigned int>(type));
}

void stop_simulation()
{
    if (g_stop)
        g_stop();
}

std::wstring dll_folder()
{
    // Find the module that contains this very function, i.e. this DLL,
    // whatever it was renamed to (PowerFactory loads any digexfun*.dll).
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&dll_folder), &self);

    // GetModuleFileNameW truncates silently when the buffer is too small
    // (long paths), so grow the buffer until the whole path fits.
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(self, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0)
            return L"";
        if (length < path.size()) {
            path.resize(length);
            break;
        }
        path.resize(path.size() * 2);
    }
    return path.substr(0, path.find_last_of(L"\\/"));
}

} // namespace pf
