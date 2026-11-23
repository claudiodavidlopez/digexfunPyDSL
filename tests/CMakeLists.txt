include(FetchContent)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
    URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# Fake digadm.dll, placed next to the test executable so that digexfun.dll's
# LoadLibrary("digadm.dll") finds it instead of PowerFactory's.
add_library(mock_digadm SHARED mock_digadm/digadm.cpp)
set_target_properties(mock_digadm PROPERTIES OUTPUT_NAME digadm)

add_executable(digexfun_tests
    pf_host.cpp
    test_registration.cpp
    test_dsl_api.cpp
    test_reference_counting.cpp
    test_error_handling.cpp
    test_simulation_runs.cpp)
target_link_libraries(digexfun_tests PRIVATE mock_digadm GTest::gtest_main)
# Loaded at run time like PowerFactory does, so only a build-order dependency.
add_dependencies(digexfun_tests digexfun)

# The embedded interpreter needs the Python DLLs on PATH.
set(python_on_path "PATH=path_list_prepend:${Python3_RUNTIME_LIBRARY_DIRS}")

# Each test in its own process: a fresh PowerFactory session.
gtest_discover_tests(digexfun_tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES ENVIRONMENT_MODIFICATION "${python_on_path}")

# All tests in one process: one long PowerFactory session with many
# simulation runs and script reloads in a row.
add_test(NAME AllTestsInOneSession COMMAND digexfun_tests --gtest_brief=1)
set_tests_properties(AllTestsInOneSession PROPERTIES
    ENVIRONMENT_MODIFICATION "${python_on_path}")
