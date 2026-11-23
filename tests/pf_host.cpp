#include "pf_host.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "mock_digadm/digadm.hpp"

namespace fs = std::filesystem;

namespace {
constexpr unsigned int MSG_ERR = 0x0001;

using InitFn     = void(__stdcall *)(void(__stdcall *)(const char *, unsigned int), void(__stdcall *)());
using RegisterFn = int(__stdcall *)(int, char *, int *);
using DslFn      = void(__stdcall *)();
} // namespace

PowerFactoryHost &PowerFactoryHost::instance()
{
    static PowerFactoryHost host;
    return host;
}

PowerFactoryHost::PowerFactoryHost()
{
    // One script directory per process, so parallel ctest runs don't collide.
    script_dir_ = (fs::temp_directory_path() /
                   ("digexfun_tests_" + std::to_string(GetCurrentProcessId())))
                      .string();
    fs::create_directories(script_dir_);
    write_script("CALLABLE_REGISTRY = []\n");

    // Must be set before the embedded interpreter starts. Bytecode caching is
    // disabled because tests rewrite script.py and reload it within the same
    // second, which an mtime-based .pyc check can miss.
    _putenv_s("PYTHONPATH", script_dir_.c_str());
    _putenv_s("PYTHONDONTWRITEBYTECODE", "1");

    dll_ = LoadLibraryA("digexfun.dll");
    if (!dll_)
        throw std::runtime_error("could not load digexfun.dll (error " +
                                 std::to_string(GetLastError()) + ")");

    reinterpret_cast<InitFn>(resolve("Init"))(&on_print, &on_stop);
}

FARPROC PowerFactoryHost::resolve(const char *name)
{
    FARPROC fn = GetProcAddress(dll_, name);
    if (!fn)
        throw std::runtime_error(std::string("digexfun.dll does not export ") + name);
    return fn;
}

std::vector<std::pair<std::string, int>> PowerFactoryHost::registered_functions()
{
    auto reg = reinterpret_cast<RegisterFn>(resolve("RegisterFunctions"));
    std::vector<std::pair<std::string, int>> out;
    for (int i = 0;; ++i) {
        char name[50] = {};
        int  argc     = -1;
        if (reg(i, name, &argc) != 0)
            break;
        out.emplace_back(name, argc);
    }
    return out;
}

double PowerFactoryHost::call(const char *name, std::initializer_list<double> args)
{
    auto fn = reinterpret_cast<DslFn>(resolve(name));

    mock_reset();
    for (double a : args)
        push(a);
    fn();

    if (mock_underflow_count() != 0)
        throw std::runtime_error(std::string(name) + " popped more values than it was given");
    if (mock_stack_size() != 1)
        throw std::runtime_error(std::string(name) + " left " +
                                 std::to_string(mock_stack_size()) +
                                 " values on the stack instead of exactly one return value");
    return *pop();
}

bool PowerFactoryHost::has_error() const
{
    for (const auto &m : messages_)
        if (m.type & MSG_ERR)
            return true;
    return false;
}

std::string PowerFactoryHost::all_messages() const
{
    std::string out;
    for (const auto &m : messages_)
        out += m.text + "\n";
    return out;
}

void PowerFactoryHost::clear_log()
{
    messages_.clear();
    stop_requests_ = 0;
}

void PowerFactoryHost::write_script(const std::string &source)
{
    std::ofstream(fs::path(script_dir_) / "script.py", std::ios::trunc) << source;
}

void __stdcall PowerFactoryHost::on_print(const char *msg, unsigned int type)
{
    instance().messages_.push_back({msg ? msg : "", type});
}

void __stdcall PowerFactoryHost::on_stop()
{
    ++instance().stop_requests_;
}
