cmake_minimum_required(VERSION 3.26)

project(digexfunPyDSL
    VERSION 1.0.0
    DESCRIPTION "Call Python functions from PowerFactory DSL models"
    LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "digexfunPyDSL builds a Windows DLL for PowerFactory; only Windows is supported.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build against Python's stable ABI (python3.dll), so one digexfun.dll works
# with any installed Python from this version on, without rebuilding.
set(DIGEXFUN_MIN_PYTHON "3.10")
find_package(Python3 ${DIGEXFUN_MIN_PYTHON} REQUIRED COMPONENTS Interpreter Development.SABIModule)

# Keep all DLLs and executables side by side so the tests' fake digadm.dll
# is found by digexfun.dll.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(digexfun SHARED
    src/digexfun.cpp   # PowerFactory side: Init, DSL value stack
    src/dsl_api.cpp    # RegisterFunctions and the four DSL functions
    src/py_bridge.cpp  # embedded Python: script, callables, GIL
    src/digexfun.def)  # export table

target_link_libraries(digexfun PRIVATE Python3::SABIModule)
target_compile_definitions(digexfun PRIVATE Py_LIMITED_API=0x030A0000) # 3.10
target_compile_options(digexfun PRIVATE /W4 /WX /permissive- /utf-8)

option(DIGEXFUN_BUILD_TESTS "Build the tests (mock PowerFactory host)" ${PROJECT_IS_TOP_LEVEL})
if(DIGEXFUN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
