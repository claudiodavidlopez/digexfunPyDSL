// Mistakes in a DSL model or in script.py must be reported in the
// PowerFactory output window and stop the simulation, never crash
// PowerFactory or hand garbage values back to DSL.

#include <limits>

#include "dsl_test.hpp"

using ErrorHandling = DslTest;

namespace {
const char *kOneFunction = R"py(
def identity(x):
    return (x,)

CALLABLE_REGISTRY = [identity]
)py";
} // namespace

// ---- function IDs --------------------------------------------------------

TEST_F(ErrorHandling, RejectsFunctionIdBeyondTheSlotLimit)
{
    pf.write_script(kOneFunction);
    EXPECT_EQ(pf.LoadPyFun(1, 64), 0.0);
    ExpectError("function ID 64");
}

TEST_F(ErrorHandling, RejectsNegativeFunctionId)
{
    pf.write_script(kOneFunction);
    pf.LoadPyFun(1, -1);
    ExpectError("function ID -1");
}

TEST_F(ErrorHandling, RejectsFractionalFunctionId)
{
    pf.write_script(kOneFunction);
    pf.LoadPyFun(1, 1.5);
    ExpectError("function ID 1.5");
}

TEST_F(ErrorHandling, RejectsNaNFunctionId)
{
    pf.write_script(kOneFunction);
    pf.LoadPyFun(1, std::numeric_limits<double>::quiet_NaN());
    ExpectError("function ID nan");
}

TEST_F(ErrorHandling, RejectsArgumentCountAboveTheLimit)
{
    pf.write_script(kOneFunction);
    pf.LoadPyFun(5000, 0);
    ExpectError("argument count 5000 is too large");
}

TEST_F(ErrorHandling, RejectsFunctionIdBeyondTheRegistry)
{
    pf.write_script(kOneFunction);
    pf.LoadPyFun(1, 3);
    ExpectError("CALLABLE_REGISTRY has 1 entries");
}

TEST_F(ErrorHandling, RejectsCallsOnFunctionThatWasNotLoaded)
{
    pf.write_script(kOneFunction);
    pf.LoadPyFun(1, 0);

    EXPECT_EQ(pf.SetPyFunArg(1.0, 0, 1), 0.0);
    ExpectError("function ID 1 was not loaded");
    pf.clear_log();

    EXPECT_EQ(pf.CallPyFun(1), 0.0);
    ExpectError("function ID 1 was not loaded");
    pf.clear_log();

    EXPECT_EQ(pf.GetPyFunRetVal(0, 1), 0.0);
    ExpectError("function ID 1 was not loaded");
}

TEST_F(ErrorHandling, RejectsRegistryEntryThatIsNotCallable)
{
    pf.write_script("CALLABLE_REGISTRY = [42]\n");
    pf.LoadPyFun(1, 0);
    ExpectError("is not callable");
}

// ---- arguments -----------------------------------------------------------

TEST_F(ErrorHandling, RejectsArgumentIndexOutOfRange)
{
    pf.write_script(kOneFunction);
    pf.LoadPyFun(1, 0);
    pf.SetPyFunArg(1.0, 5, 0);
    ExpectError("argument index 5");
}

TEST_F(ErrorHandling, RejectsCallWithArgumentsThatWereNeverSet)
{
    pf.write_script(R"py(
CALLABLE_REGISTRY = [lambda a, b: (a + b,)]
)py");
    pf.LoadPyFun(2, 0);
    pf.SetPyFunArg(1.0, 0, 0);
    pf.CallPyFun(0);
    ExpectError("argument 1 was never set");
}

// ---- return values -------------------------------------------------------

TEST_F(ErrorHandling, ReturnIndexOutOfRangeGivesZeroAndAnError)
{
    pf.write_script(kOneFunction);
    pf.LoadPyFun(1, 0);
    pf.SetPyFunArg(7.0, 0, 0);
    pf.CallPyFun(0);

    EXPECT_EQ(pf.GetPyFunRetVal(3, 0), 0.0);
    ExpectError("return value index 3");
}

TEST_F(ErrorHandling, FailedCallLeavesNoStaleReturnValues)
{
    pf.write_script(R"py(
def checked(x):
    if x < 0:
        raise ValueError("negative input")
    return (x,)

CALLABLE_REGISTRY = [checked]
)py");
    pf.LoadPyFun(1, 0);
    RunSteps(0, 1, 5.0);
    EXPECT_DOUBLE_EQ(pf.GetPyFunRetVal(0, 0), 5.0);
    ExpectNoErrors();

    RunSteps(0, 1, -1.0);
    ExpectError("ValueError: negative input");

    // The previous step's 5.0 must not be handed out as if it were current.
    pf.clear_log();
    EXPECT_EQ(pf.GetPyFunRetVal(0, 0), 0.0);
    ExpectError("returned 0 values");
}

TEST_F(ErrorHandling, RejectsReadingReturnValuesBeforeAnyCall)
{
    pf.write_script(kOneFunction);
    pf.LoadPyFun(1, 0);
    EXPECT_EQ(pf.GetPyFunRetVal(0, 0), 0.0);
    ExpectError("before CallPyFun");
}

TEST_F(ErrorHandling, RejectsReturnValueThatIsNotANumber)
{
    pf.write_script(R"py(
CALLABLE_REGISTRY = [lambda x: ("not a number",)]
)py");
    pf.LoadPyFun(1, 0);
    pf.SetPyFunArg(1.0, 0, 0);
    pf.CallPyFun(0);
    EXPECT_EQ(pf.GetPyFunRetVal(0, 0), 0.0);
    ExpectError("return value 0");
}

// ---- Python errors -------------------------------------------------------

TEST_F(ErrorHandling, ReportsExceptionsRaisedByTheCallableWithTraceback)
{
    pf.write_script(R"py(
def divide(a, b):
    return (a / b,)

CALLABLE_REGISTRY = [divide]
)py");
    pf.LoadPyFun(2, 0);
    pf.SetPyFunArg(1.0, 0, 0);
    pf.SetPyFunArg(0.0, 1, 0);
    pf.CallPyFun(0);

    ExpectError("ZeroDivisionError");
    ExpectError("script.py");
    ExpectError("in divide");
}

TEST_F(ErrorHandling, ReportsSyntaxErrorsInTheScript)
{
    pf.write_script("def broken(:\n");
    pf.LoadPyFun(1, 0);
    ExpectError("SyntaxError");
}

TEST_F(ErrorHandling, ReportsMissingCallableRegistry)
{
    pf.write_script("def f(x):\n    return (x,)\n");
    pf.LoadPyFun(1, 0);
    ExpectError("CALLABLE_REGISTRY");
}
