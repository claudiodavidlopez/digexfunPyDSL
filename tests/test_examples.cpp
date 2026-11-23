// The examples in examples/ run through the DLL as the paper describes.

#include <fstream>
#include <sstream>

#include "dsl_test.hpp"

namespace {

std::string ReadExample(const std::string &relative_path)
{
    std::ifstream file(std::string(DIGEXFUN_EXAMPLES_DIR) + "/" + relative_path);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

} // namespace

using Examples = DslTest;

// Paper, Section IV-A and Fig. 3: a high-impedance fault pulls the terminal
// voltage down to about 0.72 p.u.; after it clears, only part of the load
// comes back. Driven exactly as Listing 4 does it.
TEST_F(Examples, ElectronicLoadRespondsToAVoltageDip)
{
    const std::string script = ReadExample("electronic_load/script.py");
    ASSERT_FALSE(script.empty()) << "could not read the example";
    pf.write_script(script);

    // Model parameters in the example.
    const double Vd1 = 0.866, Vd2 = 0.7, frcel = 0.7, P0 = 20.0, Q0 = 10.0;

    const double pyFunID = 0;
    pf.LoadPyFun(1, pyFunID);

    struct Step {
        double V;
        double Fvl; // expected fraction of the nominal load
    };
    const Step steps[] = {
        {1.00, 1.0},                                       // before the fault
        {0.72, (0.72 - Vd2) / (Vd1 - Vd2)},                // dip: Vmin = 0.72
        {0.75, (0.72 - Vd2 + frcel * (0.75 - 0.72)) / (Vd1 - Vd2)}, // partial recovery
        {1.05, (0.72 - Vd2 + frcel * (Vd1 - 0.72)) / (Vd1 - Vd2)},  // fault cleared
        {0.50, 0.0},                                       // below Vd2: load trips
    };

    for (const Step &step : steps) {
        pf.SetPyFunArg(step.V, 0, pyFunID);
        pf.CallPyFun(pyFunID);
        EXPECT_NEAR(pf.GetPyFunRetVal(0, pyFunID), step.Fvl * P0, 1e-12) << "P at V = " << step.V;
        EXPECT_NEAR(pf.GetPyFunRetVal(1, pyFunID), step.Fvl * Q0, 1e-12) << "Q at V = " << step.V;
    }
    ExpectNoErrors();
}
