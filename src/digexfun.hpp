#pragma once

// The PowerFactory side of the DLL: the DSL value stack provided by
// digadm.dll, and the output window / stop callbacks passed to Init.
// Implemented in digexfun.cpp. None of these functions throw.

#include <string>

namespace pf {

// Message types understood by PowerFactory's output window. The values are
// those of the header in DIgSILENT's digexfun example.
enum class MessageType : unsigned int {
    None    = 0x0000,
    Error   = 0x0001,
    Warning = 0x0002,
    Info    = 0x0004,
    Pcl     = 0x0010,
};

// The DSL value stack. When DSL calls one of our functions, each pop()
// returns the next argument, first to last. The function must then push
// exactly one value, which DSL receives as the function's result.
double pop();
void   push(double value);

// Print one line to PowerFactory's output window.
void print(const std::string &message, MessageType type);

// Ask PowerFactory to stop the running simulation. The current DSL call
// still runs to completion and must still push its return value.
void stop_simulation();

// The folder this DLL was loaded from: PowerFactory's installation folder.
// Empty if it cannot be determined.
std::wstring dll_folder();

} // namespace pf
