# =============================================================================
# Project Configuration
# =============================================================================
cmake_minimum_required(VERSION 3.20)
project(sigint_sim LANGUAGES CXX)

# ---- Feature toggles -------------------------------------------------------
option(BUILD_BINDINGS  "Build Python pybind11 bindings"   OFF)
option(ENABLE_GNURADIO "Build with GNU Radio PHY support" OFF)
option(ENABLE_SOAPY    "Build with SoapySDR support"      OFF)

# ---- C++ standard ----------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)

# =============================================================================
# Third-party Dependencies (FetchContent) — declare only once
# =============================================================================
include(FetchContent)

# Check if already declared to avoid duplicate targets
if(NOT spdlog_FOUND)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.16.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

if(NOT googletest_FOUND)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

if(NOT json_FOUND)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    )
    FetchContent_MakeAvailable(json)
endif()


# =============================================================================
# Core Library: sigint_sim_core
# =============================================================================
set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/link.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/channel_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/simulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agents/random_agent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logging/logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/virtual_radio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/signal_processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/sample_processing_channel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/radio_interface.cpp
)

add_library(sigint_sim_core STATIC ${CORE_SOURCES})

target_include_directories(sigint_sim_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(sigint_sim_core PUBLIC spdlog::spdlog)
target_compile_features(sigint_sim_core PUBLIC cxx_std_23)
target_compile_options(sigint_sim_core PRIVATE -Wall -Wextra -Wpedantic)


# =============================================================================
# Executable: Headless Simulation Runner
# =============================================================================
add_executable(headless_run apps/headless_run.cpp)
target_link_libraries(headless_run PRIVATE sigint_sim_core)

# =============================================================================
# Tests
# =============================================================================
enable_testing()

# ---- Unit Tests ------------------------------------------------------------
set(UNIT_TEST_SOURCES
    tests/unit/test_channel.cpp
    tests/unit/test_node.cpp
    tests/unit/test_simulator.cpp
    tests/unit/test_signal_processor.cpp
    tests/unit/test_virtual_radio.cpp
    )
add_executable(unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE sigint_sim_core GTest::gtest_main)
add_test(NAME UnitTests COMMAND unit_tests)

# ---- Integration Tests -----------------------------------------------------
add_executable(integration_tests
    tests/integration/scenario_two_node_static.cpp
)
target_link_libraries(integration_tests PRIVATE sigint_sim_core GTest::gtest_main)
add_test(NAME IntegrationTests COMMAND integration_tests)

# ---- Regression Tests ------------------------------------------------------
add_executable(regression_tests 
    tests/regression/phy_replay.cpp
)
target_link_libraries(regression_tests PRIVATE sigint_sim_core GTest::gtest_main)
add_test(NAME RegressionTests COMMAND regression_tests)

# ---- Fidelity Tests ------------------------------------------------------
add_executable(fidelity_tests
    tests/fidelity/test_phy_math.cpp
)
target_link_libraries(fidelity_tests PRIVATE sigint_sim_core GTest::gtest_main)
add_test(NAME FidelityTests COMMAND fidelity_tests)

# =============================================================================
# Custom Target: Run All Tests
# =============================================================================

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS unit_tests integration_tests regression_tests fidelity_tests
)

# =============================================================================
# Python Bindings (optional)
# =============================================================================
if(BUILD_BINDINGS)
    add_subdirectory(bindings)
endif()

# =============================================================================
# Build Summary
# =============================================================================
message(STATUS "")
message(STATUS "=== Build Configuration Summary ===")
message(STATUS "CMAKE_BUILD_TYPE : ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard     : ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler         : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "BUILD_BINDINGS   : ${BUILD_BINDINGS}")

message(STATUS "Test targets:")
if(TARGET unit_tests)
    message(STATUS "  - unit_tests")
endif()
if(TARGET integration_tests)
    message(STATUS "  - integration_tests")
endif()
if(TARGET regression_tests)
    message(STATUS "  - regression_tests")
endif()
if(TARGET fidelity_tests)
    message(STATUS "  - fidelity_tests")
endif()
message(STATUS "===================================")