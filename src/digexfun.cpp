#include "digexfun.hpp"

#include <mutex>

#include <windows.h>

namespace {

using PushFn  = void(__cdecl *)(double);
using PopFn   = double *(__cdecl *)(void);
using PrintFn = void(__stdcall *)(const char *, unsigned int);
using StopFn  = void(__stdcall *)(void);

PushFn  g_push  = nullptr;
PopFn   g_pop   = nullptr;
PrintFn g_print = nullptr;
StopFn  g_stop  = nullptr;

// digadm.dll belongs to PowerFactory and is already loaded in the process.
// It is resolved on first use rather than in DllMain, where loading
// libraries is unsafe, and the reference is never released: PowerFactory
// keeps the library loaded for its whole lifetime anyway.
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

// Called by PowerFactory right after loading the DLL.
extern "C" void __stdcall Init(PrintFn print, StopFn stop)
{
    g_print = print;
    g_stop  = stop;
}

namespace pf {

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

} // namespace pf
