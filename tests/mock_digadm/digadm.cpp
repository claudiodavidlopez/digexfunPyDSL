// Stand-in for PowerFactory's digadm.dll.
//
// digexfun.dll loads "digadm.dll" by name and resolves push, pop,
// get_wrk_path and emitMessage from it. PowerFactory uses these to pass
// DSL function arguments and return values through a stack of doubles.
// This mock implements the same exports, plus a few mock_* functions that
// let tests inspect the stack.

#include <cstddef>
#include <string>
#include <vector>

#define DIGADM_EXPORT extern "C" __declspec(dllexport)

namespace {
std::vector<double> g_stack;
double              g_popped    = 0.0;
int                 g_underflow = 0;
std::string         g_wrk_path  = ".";
std::string         g_emitted;
} // namespace

// ---- exports used by digexfun.dll ----------------------------------------

DIGADM_EXPORT void push(double value)
{
    g_stack.push_back(value);
}

// The real digadm.dll returns a pointer to the popped value.
DIGADM_EXPORT double *pop(void)
{
    if (g_stack.empty()) {
        ++g_underflow;
        g_popped = 0.0;
    } else {
        g_popped = g_stack.back();
        g_stack.pop_back();
    }
    return &g_popped;
}

DIGADM_EXPORT const char *get_wrk_path(void)
{
    return g_wrk_path.c_str();
}

DIGADM_EXPORT void emitMessage(const char *msg)
{
    g_emitted += msg ? msg : "";
    g_emitted += '\n';
}

// ---- test hooks ----------------------------------------------------------

DIGADM_EXPORT void mock_reset(void)
{
    g_stack.clear();
    g_popped    = 0.0;
    g_underflow = 0;
    g_emitted.clear();
}

DIGADM_EXPORT std::size_t mock_stack_size(void)
{
    return g_stack.size();
}

DIGADM_EXPORT int mock_underflow_count(void)
{
    return g_underflow;
}
