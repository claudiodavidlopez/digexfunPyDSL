#pragma once

#include <string>

#include <gtest/gtest.h>

#include "pf_host.hpp"

// Base fixture: access to the mock PowerFactory host, with a clean log.
class DslTest : public ::testing::Test {
protected:
    PowerFactoryHost &pf = PowerFactoryHost::instance();

    void SetUp() override { pf.clear_log(); }

    void ExpectNoErrors()
    {
        EXPECT_FALSE(pf.has_error()) << pf.all_messages();
        EXPECT_EQ(pf.stop_requests(), 0) << pf.all_messages();
    }

    // An error was printed, the simulation was asked to stop, and the
    // printed text contains `needle`.
    void ExpectError(const std::string &needle)
    {
        EXPECT_TRUE(pf.has_error()) << "no error was printed";
        EXPECT_GE(pf.stop_requests(), 1) << pf.all_messages();
        EXPECT_NE(pf.all_messages().find(needle), std::string::npos)
            << "expected \"" << needle << "\" in:\n" << pf.all_messages();
    }

    // Run `steps` time steps of a one-argument callable, as a DSL model would.
    void RunSteps(int id, int steps, double arg = 1.0)
    {
        for (int i = 0; i < steps; ++i) {
            pf.SetPyFunArg(id, 0, arg);
            pf.CallPyFun(id);
        }
    }
};
