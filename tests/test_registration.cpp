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
