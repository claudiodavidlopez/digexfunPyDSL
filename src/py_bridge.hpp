#pragma once

// The embedded Python interpreter: owning references, the GIL, the user's
// script.py and the callables in its CALLABLE_REGISTRY.
//
// Everything here that touches Python objects (including destroying a
// PyRef) must run while a GilLock is held. Failures are reported by
// throwing Error. Only the stable ABI (Py_LIMITED_API) is used, so the
// DLL works with any Python from 3.10 on; see CMakeLists.txt.

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

// An owned (strong) reference to a Python object, released on destruction.
// Move-only, like std::unique_ptr. May be empty (null), e.g. when the API
// call that should have produced it failed.
class PyRef {
public:
    PyRef() = default;
    // Take ownership of a new reference, as returned by most API calls
    // ("Return value: New reference" in the Python docs).
    static PyRef steal(PyObject *object) { return PyRef(object); }
    // Add our own reference to an object owned elsewhere ("Borrowed
    // reference" in the Python docs).
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

    // The object, still owned by this PyRef (a borrowed reference).
    PyObject *get() const { return object_; }
    explicit operator bool() const { return object_ != nullptr; }
    // Release the reference now and become empty.
    void reset() { Py_CLEAR(object_); }

private:
    explicit PyRef(PyObject *object) : object_(object) {}
    PyObject *object_ = nullptr;
};

// Holds the GIL for its lifetime, starting the interpreter on first use.
// Anything that touches Python objects needs one, since PowerFactory may
// call from any thread. Safe to nest within one thread.
class GilLock {
public:
    GilLock();
    ~GilLock();
    GilLock(const GilLock &)            = delete;
    GilLock &operator=(const GilLock &) = delete;

private:
    PyGILState_STATE state_;
};

// The user's script.py and its CALLABLE_REGISTRY. All methods require the
// GIL.
class Script {
public:
    static constexpr const char *module_name   = "script";
    static constexpr const char *registry_name = "CALLABLE_REGISTRY";

    // Import a fresh copy of the script, discarding the previous one, and
    // check that it defines CALLABLE_REGISTRY as a sequence. Throws Error
    // (with the Python traceback, if there is one) on failure, after which
    // the script counts as not loaded.
    void load();
    // Number of entries in CALLABLE_REGISTRY; 0 if not loaded.
    std::size_t size() const;
    // CALLABLE_REGISTRY[index]. Throws Error if it is not callable. The
    // caller must check index < size() first.
    PyRef callable(std::size_t index) const;

private:
    PyRef module_;
    PyRef registry_;
};

// A registry entry as used by one DSL function ID: its arguments, set one
// by one from DSL, and the numbers it returned from the last call. Only
// call() touches Python and needs the GIL; the rest is plain C++.
class Callable {
public:
    // `registry_index` is only used in error messages.
    Callable(PyRef function, std::size_t registry_index, std::size_t arg_count);

    std::size_t arg_count() const { return args_.size(); }
    // The caller must check index < arg_count().
    void set_arg(std::size_t index, double value);

    // Call the function with the current arguments and store what it
    // returned as numbers in results(). Throws Error if an argument was
    // never set, if the function raises, or if a returned value is not a
    // number; results() is then empty.
    void call();
    // Whether call() has been attempted (even if it failed).
    bool called() const { return called_; }
    const std::vector<double> &results() const { return results_; }

private:
    std::vector<double> to_numbers(PyObject *result) const;

    PyRef               function_;
    std::size_t         registry_index_;
    std::vector<double> args_;    // kept between calls; DSL sets them one by one
    std::vector<bool>   arg_set_; // which args_ have been set at least once
    bool                called_ = false;
    std::vector<double> results_;
};

} // namespace digexfun
