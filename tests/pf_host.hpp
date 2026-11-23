#pragma once

// A minimal stand-in for PowerFactory as a host of digexfun.dll.
//
// It loads the DLL at run time (as PowerFactory does), calls Init with
// print/stop callbacks, and invokes the exported DSL functions using the
// digadm value-stack convention: arguments are pushed left to right, the
// function pops them, then pushes exactly one return value.

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

    // Call an exported DSL function, e.g. call("SetPyFunArg", {0, 1, 3.5}).
    double call(const char *name, std::initializer_list<double> args);

    // Convenience wrappers named after the DSL functions.
    double LoadPyFun(double id, double arg_count) { return call("LoadPyFun", {id, arg_count}); }
    double SetPyFunArg(double id, double arg, double value) { return call("SetPyFunArg", {id, arg, value}); }
    double CallPyFun(double id) { return call("CallPyFun", {id}); }
    double GetPyFunRetVal(double id, double ret) { return call("GetPyFunRetVal", {id, ret}); }

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
