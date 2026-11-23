cmake_minimum_required(VERSION 3.26)

project(digexfunPyDSL
    VERSION 1.0.0
    DESCRIPTION "Call Python functions from PowerFactory DSL models"
    LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "digexfunPyDSL builds a Windows DLL for PowerFactory; only Windows is supported.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Embed)

# Keep all DLLs and executables side by side so the tests' fake digadm.dll
# is found by digexfun.dll.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(digexfun SHARED
    src/digexfun.cpp
    src/userfun.cpp
    src/cpppy.cpp
    src/digexfun.def)

target_link_libraries(digexfun PRIVATE Python3::Python)
target_compile_definitions(digexfun PRIVATE _CRT_SECURE_NO_WARNINGS)

option(DIGEXFUN_BUILD_TESTS "Build the tests (mock PowerFactory host)" ${PROJECT_IS_TOP_LEVEL})
if(DIGEXFUN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
