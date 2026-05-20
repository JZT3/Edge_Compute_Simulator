# bindings/CMakeLists.txt
# Must be included from top-level CMakeLists.txt with BUILD_BINDINGS=ON

if(NOT TARGET sigint_sim_core)
    message(FATAL_ERROR "sigint_sim_core library must be built before bindings")
endif()

# Find or fetch pybind11
find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND)
    message(STATUS "Fetching pybind11...")
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.11.1
    )
    FetchContent_MakeAvailable(pybind11)
endif()

pybind11_add_module(_sigint_sim_core bindings.cpp)

target_link_libraries(_sigint_sim_core PRIVATE sigint_sim_core)

# Compile with C++23 and hide internal symbols
target_compile_features(_sigint_sim_core PRIVATE cxx_std_23)
set_target_properties(_sigint_sim_core PROPERTIES CXX_VISIBILITY_PRESET hidden)

# Release the GIL during step() calls (handled in source, but we set a note)
# The bindings.cpp uses py::call_guard<py::gil_scoped_release> on step().

# Install the .so next to the Python package for development?
# In development, set CMAKE_LIBRARY_OUTPUT_DIRECTORY to gui/src/sigint_gui/
# so the module is importable directly.
set_target_properties(_sigint_sim_core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/gui/src/sigint_gui"
)