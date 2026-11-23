#pragma once

// The PowerFactory side of the DLL: the DSL value stack provided by
// digadm.dll, and the output window / stop callbacks passed to Init.

#include <string>

namespace pf {

// Message types understood by PowerFactory's output window.
enum class MessageType : unsigned int {
    None    = 0x0000,
    Error   = 0x0001,
    Warning = 0x0002,
    Info    = 0x0004,
    Pcl     = 0x0010,
};

// DSL arguments arrive on a stack of doubles: each pop() returns the next
// argument, first to last. Push exactly one return value afterwards.
double pop();
void   push(double value);

void print(const std::string &message, MessageType type);
void stop_simulation();

// The folder this DLL was loaded from: PowerFactory's installation folder.
std::wstring dll_folder();

} // namespace pf
