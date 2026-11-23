#pragma once

// The embedded Python interpreter: owning references, the GIL, the user's
// script.py and the callables in its CALLABLE_REGISTRY.

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "python.hpp"

namespace digexfun {

// A failure to report to the user; the message is ready to display.
class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Throw an Error with `what`, followed by the pending Python exception and
// its traceback (which is cleared).
[[noreturn]] void throw_python_error(const std::string &what);

// An owned (strong) reference to a Python object.
class PyRef {
public:
    PyRef() = default;
    // Take ownership of a new reference, as returned by most API calls.
    static PyRef steal(PyObject *object) { return PyRef(object); }
    // Add a reference to an object owned elsewhere.
    static PyRef borrow(PyObject *object)
    {
        Py_XINCREF(object);
        return PyRef(object);
    }

    PyRef(PyRef &&other) noexcept : object_(std::exchange(other.object_, nullptr)) {}
    PyRef &operator=(PyRef &&other) noexcept
    {
        if (this != &other) {
            reset();
            object_ = std::exchange(other.object_, nullptr);
        }
        return *this;
    }
    PyRef(const PyRef &)            = delete;
    PyRef &operator=(const PyRef &) = delete;
    ~PyRef() { reset(); }

    PyObject *get() const { return object_; }
    explicit operator bool() const { return object_ != nullptr; }
    void reset() { Py_CLEAR(object_); }

private:
    explicit PyRef(PyObject *object) : object_(object) {}
    PyObject *object_ = nullptr;
};

// Holds the GIL for its lifetime, starting the interpreter on first use.
// Anything that touches Python objects needs one, since PowerFactory may
// call from any thread.
class GilLock {
public:
    GilLock();
    ~GilLock();
    GilLock(const GilLock &)            = delete;
    GilLock &operator=(const GilLock &) = delete;

private:
    PyGILState_STATE state_;
};

// The user's script.py and its CALLABLE_REGISTRY.
class Script {
public:
    static constexpr const char *module_name   = "script";
    static constexpr const char *registry_name = "CALLABLE_REGISTRY";

    // Import a fresh copy of the script, discarding the previous one.
    void load();
    std::size_t size() const;
    PyRef callable(std::size_t index) const;

private:
    PyRef module_;
    PyRef registry_;
};

// A registry entry as used by one DSL function ID: its arguments, set one
// by one from DSL, and the numbers it returned from the last call.
class Callable {
public:
    Callable(PyRef function, std::size_t registry_index, std::size_t arg_count);

    std::size_t arg_count() const { return args_.size(); }
    void set_arg(std::size_t index, double value);

    void call();
    bool called() const { return called_; }
    const std::vector<double> &results() const { return results_; }

private:
    std::vector<double> to_numbers(PyObject *result) const;

    PyRef               function_;
    std::size_t         registry_index_;
    std::vector<double> args_;
    std::vector<bool>   arg_set_;
    bool                called_ = false;
    std::vector<double> results_;
};

} // namespace digexfun
