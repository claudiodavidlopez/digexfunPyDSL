// PowerFactory keeps the DLL loaded between simulation runs. Each run
// starts with the LoadPyFun calls in the DSL blocks' initialisation;
// loading an ID that is already loaded starts a new run, which reloads
// script.py so edits are picked up and Python state starts fresh.

#include "dsl_test.hpp"

using SimulationRuns = DslTest;

namespace {
const char *kIntegrator = R"py(
class Integrator:
    def __init__(self, gain):
        self.gain = gain
        self.total = 0.0

    def step(self, x):
        self.total += self.gain * x
        return (self.total,)

CALLABLE_REGISTRY = [Integrator(gain=GAIN).step]
)py";

std::string IntegratorWithGain(double gain)
{
    return "GAIN = " + std::to_string(gain) + "\n" + kIntegrator;
}
} // namespace

TEST_F(SimulationRuns, RerunReloadsTheScriptAndResetsPythonState)
{
    pf.write_script(IntegratorWithGain(1.0));
    pf.LoadPyFun(0, 1);
    RunSteps(0, 100);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 100.0);

    pf.write_script(IntegratorWithGain(2.0));
    pf.LoadPyFun(0, 1);
    RunSteps(0, 3);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 6.0);
    ExpectNoErrors();
}

TEST_F(SimulationRuns, RerunDoesNotKeepNamesRemovedFromTheScript)
{
    const char *registry = R"py(
CALLABLE_REGISTRY = [lambda x: (globals().get("OFFSET", 0.0) + x,)]
)py";
    pf.write_script(std::string("OFFSET = 10.0\n") + registry);
    pf.LoadPyFun(0, 1);
    RunSteps(0, 1, 1.0);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 11.0);

    pf.write_script(registry);
    pf.LoadPyFun(0, 1);
    RunSteps(0, 1, 1.0);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 1.0);
    ExpectNoErrors();
}

TEST_F(SimulationRuns, RerunWorksAfterAShortRun)
{
    pf.write_script(IntegratorWithGain(1.0));
    pf.LoadPyFun(0, 1);
    RunSteps(0, 2);

    pf.LoadPyFun(0, 1);
    RunSteps(0, 2);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 2.0);
    ExpectNoErrors();
}

TEST_F(SimulationRuns, InitialisingTwiceWithoutSimulatingIsAllowed)
{
    pf.write_script(IntegratorWithGain(1.0));
    pf.LoadPyFun(0, 1);
    pf.LoadPyFun(0, 1);
    RunSteps(0, 1);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 1.0);
    ExpectNoErrors();
}

TEST_F(SimulationRuns, RerunForgetsCallablesOfThePreviousRun)
{
    pf.write_script(R"py(
CALLABLE_REGISTRY = [lambda x: (x,), lambda x: (-x,)]
)py");
    pf.LoadPyFun(0, 1);
    pf.LoadPyFun(1, 1);
    RunSteps(0, 1);
    RunSteps(1, 1);

    // The new run's model only uses function 0.
    pf.LoadPyFun(0, 1);
    pf.CallPyFun(1);
    ExpectError("function ID 1 was not loaded");
}

TEST_F(SimulationRuns, RecoversAfterAnErrorStoppedThePreviousRun)
{
    pf.write_script(R"py(
CALLABLE_REGISTRY = [lambda x: (x,), "oops"]
)py");
    pf.LoadPyFun(0, 1);
    pf.LoadPyFun(1, 1);
    ExpectError("is not callable");

    // The user fixes the script and starts the simulation again.
    pf.clear_log();
    pf.write_script(R"py(
CALLABLE_REGISTRY = [lambda x: (x,), lambda x: (2 * x,)]
)py");
    pf.LoadPyFun(0, 1);
    pf.LoadPyFun(1, 1);
    RunSteps(1, 1, 4.0);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(1, 0), 8.0);
    ExpectNoErrors();
}

TEST_F(SimulationRuns, RecoversWhenNoFunctionWasLoadedSuccessfully)
{
    pf.write_script("CALLABLE_REGISTRY = [42]\n");
    pf.LoadPyFun(0, 1);
    ExpectError("is not callable");

    pf.clear_log();
    pf.write_script("CALLABLE_REGISTRY = [lambda x: (x + 1,)]\n");
    pf.LoadPyFun(0, 1);
    RunSteps(0, 1, 1.0);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 2.0);
    ExpectNoErrors();
}

TEST_F(SimulationRuns, RecoversAfterAScriptThatFailedToImport)
{
    pf.write_script("def broken(:\n");
    pf.LoadPyFun(0, 1);
    ExpectError("SyntaxError");

    pf.clear_log();
    pf.write_script(IntegratorWithGain(1.0));
    pf.LoadPyFun(0, 1);
    RunSteps(0, 1);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 1.0);
    ExpectNoErrors();
}

TEST_F(SimulationRuns, RecoversAfterAFailedReload)
{
    pf.write_script(IntegratorWithGain(1.0));
    pf.LoadPyFun(0, 1);
    RunSteps(0, 10);

    pf.write_script("def broken(:\n");
    pf.LoadPyFun(0, 1);
    ExpectError("SyntaxError");

    pf.clear_log();
    pf.write_script(IntegratorWithGain(3.0));
    pf.LoadPyFun(0, 1);
    RunSteps(0, 1);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 3.0);
    ExpectNoErrors();
}
