#pragma once

// A minimal stand-in for PowerFactory as a host of digexfun.dll.
//
// It loads the DLL at run time (as PowerFactory does), calls Init with
// print/stop callbacks, and invokes the exported DSL functions using the
// digadm value-stack convention: the arguments are pushed so that the
// function pops them first to last, then it pushes exactly one return
// value.

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

class PowerFactoryHost {
public:
    struct Message {
        std::string  text;
        unsigned int type;
    };

    // The single host instance for this process (the DLL keeps global state).
    static PowerFactoryHost &instance();

    // Registered DSL functions as (name, argument count), via RegisterFunctions.
    std::vector<std::pair<std::string, int>> registered_functions();

    // Call an exported DSL function with its arguments in DSL order,
    // e.g. call("SetPyFunArg", {3.5, 1, 0}).
    double call(const char *name, std::initializer_list<double> args);

    // The DSL functions, with the signatures documented in the paper.
    double LoadPyFun(double argNum, double pyFunID) { return call("LoadPyFun", {argNum, pyFunID}); }
    double SetPyFunArg(double argVal, double argID, double pyFunID) { return call("SetPyFunArg", {argVal, argID, pyFunID}); }
    double CallPyFun(double pyFunID) { return call("CallPyFun", {pyFunID}); }
    double GetPyFunRetVal(double retValID, double pyFunID) { return call("GetPyFunRetVal", {retValID, pyFunID}); }

    // Messages printed to the "output window", and stop requests.
    const std::vector<Message> &messages() const { return messages_; }
    int  stop_requests() const { return stop_requests_; }
    bool has_error() const;
    std::string all_messages() const;
    void clear_log();

    // Write the Python module that digexfun.dll imports as "script".
    void write_script(const std::string &source);

private:
    PowerFactoryHost();
    FARPROC resolve(const char *name);

    static void __stdcall on_print(const char *msg, unsigned int type);
    static void __stdcall on_stop();

    HMODULE              dll_ = nullptr;
    std::string          script_dir_;
    std::vector<Message> messages_;
    int                  stop_requests_ = 0;
};
