#include <gtest/gtest.h>

#include "pf_host.hpp"

TEST(Registration, ExposesTheFourDslFunctions)
{
    auto fns = PowerFactoryHost::instance().registered_functions();

    using Fn = std::pair<std::string, int>;
    EXPECT_EQ(fns, (std::vector<Fn>{
                       {"LoadPyFun", 2},
                       {"SetPyFunArg", 3},
                       {"CallPyFun", 1},
                       {"GetPyFunRetVal", 2},
                   }));
}

TEST(Registration, RejectsOutOfRangeIndicesAndNullPointers)
{
    auto &pf      = PowerFactoryHost::instance();
    char  name[50] = {};
    int   argc     = 0;

    EXPECT_NE(pf.register_function(-1, name, &argc), 0);
    EXPECT_NE(pf.register_function(4, name, &argc), 0);
    EXPECT_NE(pf.register_function(0, nullptr, &argc), 0);
    EXPECT_NE(pf.register_function(0, name, nullptr), 0);
}
