cmake_minimum_required(VERSION 3.20)
project(sigint_sim LANGUAGES CXX)

# ---- C++ Standard and Compiler Options ----
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict warnings and treat as errors
add_compile_options(-Wall -Wextra -Wpedantic -Werror)
set(CMAKE_VERBOSE_MAKEFILE ON)

# ---- Path where CMake will look for find modules ----
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ---- Options for optional components ----
option(BUILD_BINDINGS  "Build Python pybind11 bindings"   OFF)
option(ENABLE_GNURADIO "Build with GNU Radio PHY support" OFF)
option(ENABLE_SOAPY    "Build with SoapySDR support"      OFF)

# ---- C++ Standard and Compiler Options ----
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# ---- FetchContent: third-party libraries ----
include(FetchContent)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.13.0
)
FetchContent_MakeAvailable(spdlog)

# GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

# ---- Library: sigint_sim_core (all simulation code) ----

message("Current source directory: ${CMAKE_CURRENT_SOURCE_DIR}") # DEBUG

set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/link.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/channel_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/simulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agents/random_agent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logging/logger.cpp
)

set(CORE_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(sigint_sim_core STATIC ${CORE_SOURCES})

target_include_directories(sigint_sim_core 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(sigint_sim_core
    PUBLIC
        spdlog::spdlog
)

# set library properties for later installation
target_compile_features(sigint_sim_core PUBLIC cxx_std_23)

# ---- Executable: headless simulation runner ----
add_executable(headless_run apps/headless_run.cpp)
target_link_libraries(headless_run PRIVATE sigint_sim_core)

# ---- Tests ----
enable_testing()

# Unit tests
set(UNIT_TEST_SOURCES
    tests/unit/test_channel.cpp
    tests/unit/test_node.cpp
    tests/unit/test_utility.cpp
    tests/unit/test_simulator.cpp
)

add_executable(unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(unit_tests
    PRIVATE
        sigint_sim_core
        GTest::gtest_main
)
add_test(NAME UnitTests COMMAND unit_tests)

# Integration tests
set(INTEGRATION_TEST_SOURCES
    tests/integration/scenario_two_node_static.cpp
    # tests/integration/scenario_three_node_tdoa.cpp
)

add_executable(integration_tests ${INTEGRATION_TEST_SOURCES})
target_link_libraries(integration_tests
    PRIVATE
        sigint_sim_core
        GTest::gtest_main
)
add_test(NAME IntegrationTests COMMAND integration_tests)

# Regression tests
set(REGRESSION_TEST_SOURCES
    tests/regression/deterministic_replay.cpp
)
add_executable(regression_tests ${REGRESSION_TEST_SOURCES})
target_link_libraries(regression_tests
    PRIVATE
        sigint_sim_core
        GTest::gtest_main
)
add_test(NAME RegressionTests COMMAND regression_tests)

# ---- Custom target to run all tests ----
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS unit_tests integration_tests regression_tests
)

if(ENABLE_GNURADIO)
    find_package(Gnuradio REQUIRED COMPONENTS runtime)
endif()

if(ENABLE_SOAPY)
    find_package(SoapySDR REQUIRED)
endif()

if(ENABLE_GNURADIO AND ENABLE_SOAPY)
    # ---------- Locate libraries manually (if needed) ----------
    # Try to find the gnuradio-runtime library file
    find_library(GNURADIO_RUNTIME_LIBRARY NAMES gnuradio-runtime
        HINTS ${GNURADIO_ROOT} $ENV{GNURADIO_ROOT}
        PATH_SUFFIXES lib
    )
    if(NOT GNURADIO_RUNTIME_LIBRARY)
        # Fallback to pkg-config if available
        execute_process(
            COMMAND pkg-config --libs-only-L gnuradio-runtime
            OUTPUT_VARIABLE GR_LIBDIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(GR_LIBDIR)
            string(REPLACE "-L" "" GR_LIBDIR ${GR_LIBDIR})
            find_library(GNURADIO_RUNTIME_LIBRARY NAMES gnuradio-runtime
                HINTS ${GR_LIBDIR}
            )
        endif()
    endif()

    find_library(SoapySDR_LIBRARY NAMES SoapySDR
        HINTS ${SoapySDR_ROOT} $ENV{SoapySDR_ROOT}
        PATH_SUFFIXES lib
    )

    # ---------- Include directories ----------
    # Already set by find_package if successful; fallback to manual search
    if(NOT GNURADIO_ALL_INCLUDE_DIRS)
        execute_process(
            COMMAND pkg-config --cflags-only-I gnuradio-runtime
            OUTPUT_VARIABLE GR_INCLUDES
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        string(REGEX REPLACE "-I" "" GNURADIO_ALL_INCLUDE_DIRS ${GR_INCLUDES})
    endif()
    if(NOT SoapySDR_INCLUDE_DIRS)
        find_path(SoapySDR_INCLUDE_DIRS SoapySDR/Device.hpp
            HINTS ${SoapySDR_ROOT} $ENV{SoapySDR_ROOT}
            PATH_SUFFIXES include
        )
    endif()

    # ---------- Create the test target ----------
    add_executable(phase0_test tests/fidelity/test_gnuradio_soapy.cpp)

    target_include_directories(phase0_test PRIVATE
        ${GNURADIO_ALL_INCLUDE_DIRS}
        ${SoapySDR_INCLUDE_DIRS}
    )

    target_link_libraries(phase0_test
        ${GNURADIO_RUNTIME_LIBRARY}
        ${SoapySDR_LIBRARY}
    )

    message(STATUS "GNURadio runtime lib: ${GNURADIO_RUNTIME_LIBRARY}")
    message(STATUS "SoapySDR lib: ${SoapySDR_LIBRARY}")
endif()

# ---- Bindings (Python) ----
if(BUILD_BINDINGS)
    add_subdirectory(bindings)
endif()