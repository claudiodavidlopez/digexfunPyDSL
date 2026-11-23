// Behaviour of the four DSL functions as seen from a DSL model.

#include "dsl_test.hpp"

using DslApi = DslTest;

TEST_F(DslApi, CallsFunctionAndReturnsTupleValues)
{
    pf.write_script(R"py(
def sum_and_product(a, b):
    return a + b, a * b

CALLABLE_REGISTRY = [sum_and_product]
)py");

    pf.LoadPyFun(2, 0);
    pf.SetPyFunArg(2.0, 0, 0);
    pf.SetPyFunArg(3.5, 1, 0);
    pf.CallPyFun(0);

    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 5.5);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(1, 0), 7.0);
    ExpectNoErrors();
}

TEST_F(DslApi, KeepsStateAcrossTimeStepsInBoundMethods)
{
    pf.write_script(R"py(
class Integrator:
    def __init__(self):
        self.total = 0.0

    def step(self, x):
        self.total += x
        return (self.total,)

integrator = Integrator()
CALLABLE_REGISTRY = [integrator.step]
)py");

    pf.LoadPyFun(1, 0);
    for (int step = 1; step <= 20; ++step) {
        pf.SetPyFunArg(0.5, 0, 0);
        pf.CallPyFun(0);
        EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 0.5 * step);
    }
    ExpectNoErrors();
}

TEST_F(DslApi, SelectsCallablesByIndexInRegistry)
{
    pf.write_script(R"py(
def double(x):
    return (2.0 * x,)

def negate(x):
    return (-x,)

CALLABLE_REGISTRY = [double, negate]
)py");

    pf.LoadPyFun(1, 0);
    pf.LoadPyFun(1, 1);

    pf.SetPyFunArg(4.0, 0, 0);
    pf.SetPyFunArg(4.0, 0, 1);
    pf.CallPyFun(0);
    pf.CallPyFun(1);

    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 8.0);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 1), -4.0);
    ExpectNoErrors();
}

TEST_F(DslApi, CallsFunctionWithoutArguments)
{
    pf.write_script(R"py(
def constant():
    return (42.0,)

CALLABLE_REGISTRY = [constant]
)py");

    pf.LoadPyFun(0, 0);
    pf.CallPyFun(0);

    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 42.0);
    ExpectNoErrors();
}

TEST_F(DslApi, EveryCallReturnsZeroToDsl)
{
    pf.write_script(R"py(
CALLABLE_REGISTRY = [lambda x: (x,)]
)py");

    EXPECT_EQ(pf.LoadPyFun(1, 0), 0.0);
    EXPECT_EQ(pf.SetPyFunArg(1.0, 0, 0), 0.0);
    EXPECT_EQ(pf.CallPyFun(0), 0.0);
    ExpectNoErrors();
}
