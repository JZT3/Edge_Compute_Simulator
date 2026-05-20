cmake_minimum_required(VERSION 3.20)
project(sigint_sim LANGUAGES CXX)

option(BUILD_BINDINGS "Build Python pybind11 bindings" OFF)

# ---- C++ Standard and Compiler Options ----
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict warnings and treat as errors
add_compile_options(-Wall -Wextra -Wpedantic -Werror)
set(CMAKE_VERBOSE_MAKEFILE ON)


# ---- FetchContent: third-party libraries ----
include(FetchContent)

# spdlog (header-only, optionally compiled)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.13.0
)
FetchContent_MakeAvailable(spdlog)

# GoogleTest for unit and integration tests
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

message("Current source directory: ${CMAKE_CURRENT_SOURCE_DIR}")



# ---- Library: sigint_sim_core (all simulation code) ----
set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/link.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/channel_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/simulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agents/random_agent.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/agents/potential_game_agent.cpp   # will be added later
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/agents/qlearning_agent.cpp        # will be added later
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/game/utility.cpp                  # later
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/game/potential.cpp                # later
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logging/logger.cpp
)

set(CORE_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(sigint_sim_core STATIC ${CORE_SOURCES})

target_include_directories(sigint_sim_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)


target_link_libraries(sigint_sim_core
    PUBLIC
        spdlog::spdlog
)

# Optional: set library properties for later installation
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
    # tests/unit/test_potential_game_agent.cpp
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

if(BUILD_BINDINGS)
    add_subdirectory(bindings)
endif()

# ---- (Optional) Sphinx documentation target ----
# find_package(Sphinx)
# if(SPHINX_FOUND)
#     add_custom_target(docs ...)
# endif()