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

add_library(digexfun SHARED
    src/digexfun.cpp
    src/userfun.cpp
    src/cpppy.cpp
    src/digexfun.def)

target_link_libraries(digexfun PRIVATE Python3::Python)
target_compile_definitions(digexfun PRIVATE _CRT_SECURE_NO_WARNINGS)
