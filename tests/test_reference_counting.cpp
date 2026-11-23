// Python objects must not leak, or be released too early, over a
// simulation (thousands of steps) or over many simulation runs in one
// PowerFactory session.

#include "dsl_test.hpp"

using ReferenceCounting = DslTest;

TEST_F(ReferenceCounting, ReleasesReturnValuesEveryStep)
{
    // Callable 0 always returns the same tuple object; callable 1 reports how
    // many references to that tuple exist. If the DLL keeps every returned
    // tuple alive, the count grows with each step.
    pf.write_script(R"py(
import sys

RESULT = (1.0,)

def constant(x):
    return RESULT

def refcount_of_result(x):
    return (float(sys.getrefcount(RESULT)),)

CALLABLE_REGISTRY = [constant, refcount_of_result]
)py");

    pf.LoadPyFun(0, 1);
    pf.LoadPyFun(1, 1);

    RunSteps(0, 1);
    RunSteps(1, 1);
    const double before = pf.GetPyFunRetVal(1, 0);

    RunSteps(0, 500);
    RunSteps(1, 1);
    const double after = pf.GetPyFunRetVal(1, 0);

    EXPECT_EQ(after, before) << "returned tuples are leaking";
    ExpectNoErrors();
}

TEST_F(ReferenceCounting, KeepsRegistryEntriesAliveAcrossRuns)
{
    // The callable lives in builtins so it survives reloading the script.
    // Its reference count must be the same after every run; a drift means
    // the DLL is releasing references it doesn't own, or leaking them.
    pf.write_script(R"py(
import builtins
import sys

class Probe:
    def __call__(self, x):
        return (float(sys.getrefcount(self)),)

if not hasattr(builtins, "digexfun_probe"):
    builtins.digexfun_probe = Probe()

CALLABLE_REGISTRY = [builtins.digexfun_probe]
)py");

    double first_run_count = 0.0;
    for (int run = 0; run < 20; ++run) {
        pf.LoadPyFun(0, 1);
        RunSteps(0, 10);
        const double count = pf.GetPyFunRetVal(0, 0);
        if (run == 0)
            first_run_count = count;
        EXPECT_EQ(count, first_run_count) << "in run " << run;
    }
    ExpectNoErrors();
}
