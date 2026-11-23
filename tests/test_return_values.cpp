// Callables may return what Python code naturally returns: a tuple or
// list of numbers, a single number, or None. Anything float() accepts
// counts as a number (int, bool, numpy scalars, ...).

#include "dsl_test.hpp"

using ReturnValues = DslTest;

TEST_F(ReturnValues, SingleNumberIsOneReturnValue)
{
    pf.write_script("CALLABLE_REGISTRY = [lambda x: 2 * x]\n");
    pf.LoadPyFun(0, 1);
    RunSteps(0, 1, 21.0);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 42.0);
    ExpectNoErrors();
}

TEST_F(ReturnValues, IntegersAndBooleansAreNumbers)
{
    pf.write_script("CALLABLE_REGISTRY = [lambda: (3, True, -7)]\n");
    pf.LoadPyFun(0, 0);
    pf.CallPyFun(0);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 3.0);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 1), 1.0);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 2), -7.0);
    ExpectNoErrors();
}

TEST_F(ReturnValues, ObjectsConvertibleWithFloatAreNumbers)
{
    // Stands in for numpy.float64 and similar scalar types.
    pf.write_script(R"py(
class Scalar:
    def __init__(self, value):
        self.value = value

    def __float__(self):
        return float(self.value)

CALLABLE_REGISTRY = [lambda: (Scalar(1.5), Scalar(-2))]
)py");
    pf.LoadPyFun(0, 0);
    pf.CallPyFun(0);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 1.5);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 1), -2.0);
    ExpectNoErrors();
}

TEST_F(ReturnValues, ListsWorkLikeTuples)
{
    pf.write_script("CALLABLE_REGISTRY = [lambda: [0.25, 0.5]]\n");
    pf.LoadPyFun(0, 0);
    pf.CallPyFun(0);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 0.25);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 1), 0.5);
    ExpectNoErrors();
}

TEST_F(ReturnValues, NoneMeansNoReturnValues)
{
    pf.write_script(R"py(
log = []
CALLABLE_REGISTRY = [lambda x: log.append(x)]
)py");
    pf.LoadPyFun(0, 1);
    RunSteps(0, 3);
    ExpectNoErrors();

    EXPECT_EQ(pf.GetPyFunRetVal(0, 0), 0.0);
    ExpectError("returned 0 values");
}

TEST_F(ReturnValues, StringsAreNotSequencesOfNumbers)
{
    pf.write_script("CALLABLE_REGISTRY = [lambda: \"12\"]\n");
    pf.LoadPyFun(0, 0);
    pf.CallPyFun(0);
    ExpectError("return value 0");
}

TEST_F(ReturnValues, ConversionErrorNamesTheOffendingValue)
{
    pf.write_script("CALLABLE_REGISTRY = [lambda: (1.0, 2.0, {})]\n");
    pf.LoadPyFun(0, 0);
    pf.CallPyFun(0);
    ExpectError("return value 2");
    ExpectError("dict");
}
