// script.py goes in PowerFactory's installation folder, next to the DLL
// (see the paper, Section II-B). That folder is searched first, whatever
// PowerFactory's working directory is; PYTHONPATH also works.

#include "dsl_test.hpp"

using ScriptLocation = DslTest;

TEST_F(ScriptLocation, DllFolderIsSearchedFirst)
{
    // The tests' own scripts are found through PYTHONPATH, so this script
    // only reports what sys.path looks like.
    pf.write_script(R"py(
import os
import sys

DLL_FOLDER = r")py" + pf.dll_folder() + R"py("

def same(a, b):
    return os.path.normcase(os.path.abspath(a)) == os.path.normcase(os.path.abspath(b))

CALLABLE_REGISTRY = [lambda: float(same(sys.path[0], DLL_FOLDER))]
)py");

    pf.LoadPyFun(0, 0);
    pf.CallPyFun(0);
    EXPECT_EQ(pf.GetPyFunRetVal(0, 0), 1.0) << "sys.path[0] is not " << pf.dll_folder();
    ExpectNoErrors();
}
