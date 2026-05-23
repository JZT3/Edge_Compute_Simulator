#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------------------------
#  sigint_sim – simple build & test script
#  Usage:
#    ./build.sh             # release, no bindings
#    ./build.sh -c          # clean build directory first
#    ./build.sh full        # release + bindings + GUI install
#    ./build.sh full -c     # full build, clean first
#    ./build.sh debug       # debug build, no bindings
# ---------------------------------------------------------------------------

BUILD_TYPE="${1:-release}"
CLEAN_FLAG="${2:-}"

# Normalise arguments: if first arg is -c, it's a clean request for release
if [[ "$BUILD_TYPE" == "-c" ]]; then
    BUILD_TYPE="release"
    CLEAN_FLAG="-c"
    rm -rf output/
    echo "Output directory cleaned"
fi

BUILD_DIR="build"

# Clean if requested
if [[ "$CLEAN_FLAG" == "-c" ]]; then
    rm -rf "$BUILD_DIR"
    echo "Build directory cleaned"
fi

# CMake flags
CMAKE_FLAGS=(-S . -B "$BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
case "$BUILD_TYPE" in
    debug)
        CMAKE_FLAGS+=(-DCMAKE_BUILD_TYPE=Debug)
        ;;
    full)
        CMAKE_FLAGS+=(-DCMAKE_BUILD_TYPE=Release -DBUILD_BINDINGS=ON)
        ;;
    *)
        CMAKE_FLAGS+=(-DCMAKE_BUILD_TYPE=Release)
        ;;
esac

echo "Configuring ($BUILD_TYPE)..."
cmake "${CMAKE_FLAGS[@]}"

# Build with all cores
NPROC=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
echo "Building (parallel, $NPROC jobs)..."
cmake --build "$BUILD_DIR" --parallel "$NPROC"

# Run all test suites
echo ""
echo "Running tests..."
FAILED=0
for TEST in unit_tests integration_tests regression_tests fidelity_tests; do
    if [[ -x "$BUILD_DIR/$TEST" ]]; then
        echo "--- $TEST ---"
        if "$BUILD_DIR/$TEST"; then
            echo "$TEST passed"
        else
            echo "$TEST failed"
            FAILED=1
        fi
    fi
done

if [[ $FAILED -eq 0 ]]; then
    echo "All tests passed."
else
    echo "Some tests failed. Check output above."
fi

# Install Python GUI if full build and bindings were built
if [[ "$BUILD_TYPE" == "full" ]]; then
    echo ""
    echo "Installing GUI (editable)..."
    cd gui
    pip install -e .
    cd ..
    echo "GUI installed."
fi

echo ""
echo "Build finished."