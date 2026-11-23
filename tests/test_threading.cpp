// The DLL must not assume that PowerFactory calls it from the thread that
// loaded it: every entry point acquires the Python GIL itself.

#include <thread>

#include "dsl_test.hpp"

using Threading = DslTest;

namespace {
// Returns the number of calls so far and the ID of the OS thread the
// Python code ran on (threading.get_ident() is the Windows thread ID).
const char *kCounter = R"py(
import threading

calls = 0

def count():
    global calls
    calls += 1
    return (float(calls), float(threading.get_ident()))

CALLABLE_REGISTRY = [count]
)py";
} // namespace

TEST_F(Threading, WorksWhenCalledFromAnotherThread)
{
    pf.write_script(kCounter);

    DWORD simulation_thread = 0;
    std::thread simulation([&] {
        simulation_thread = GetCurrentThreadId();
        pf.LoadPyFun(0, 0);
        for (int i = 0; i < 10; ++i)
            pf.CallPyFun(0);
    });
    simulation.join();

    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 10.0);
    EXPECT_EQ(pf.GetPyFunRetVal(1, 0), static_cast<double>(simulation_thread))
        << "the Python code did not run on the calling thread";
    EXPECT_NE(simulation_thread, GetCurrentThreadId());
    ExpectNoErrors();
}

TEST_F(Threading, WorksWhenSuccessiveRunsUseDifferentThreads)
{
    pf.write_script(kCounter);

    for (int run = 0; run < 3; ++run) {
        DWORD simulation_thread = 0;
        std::thread simulation([&] {
            simulation_thread = GetCurrentThreadId();
            pf.LoadPyFun(0, 0);
            pf.CallPyFun(0);
            pf.CallPyFun(0);
        });
        simulation.join();
        EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 2.0) << "in run " << run;
        EXPECT_EQ(pf.GetPyFunRetVal(1, 0), static_cast<double>(simulation_thread))
            << "in run " << run;
    }
    ExpectNoErrors();
}
