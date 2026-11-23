#pragma once

#include <cstddef>

// Declarations for the mock digadm.dll (see digadm.cpp).
extern "C" {
__declspec(dllimport) void        push(double value);
__declspec(dllimport) double     *pop(void);
__declspec(dllimport) void        mock_reset(void);
__declspec(dllimport) std::size_t mock_stack_size(void);
__declspec(dllimport) int         mock_underflow_count(void);
}
