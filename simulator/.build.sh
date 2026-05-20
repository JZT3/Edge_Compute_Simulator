#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------------------------
#  sigint_sim – one-command build script
# ---------------------------------------------------------------------------
#  Usage:
#    ./build.sh                    # release build, no GNU Radio/Soapy, no bindings
#    ./build.sh debug              # debug build
#    ./build.sh full               # release + bindings + Python GUI
#    ./build.sh all                # release + bindings + Python GUI + GNU Radio + SoapySDR
#    ./build.sh <type> clean       # clean build directory first
#    ./build.sh <type> parallel    # use parallel build (for slow builds)
# ---------------------------------------------------------------------------

BUILD_TYPE="${1:-release}"   # release | debug | full | all
EXTRA_FLAG="${2:-}"          # clean | parallel
BUILD_DIR="build"
GUI_DIR="gui"

# ---- helper ---------------------------------------------------------------
run_cmake() {
    local extra_flags=("$@")
    cmake -S . -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="${2:-Release}" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        "${extra_flags[@]}"
}

# ---- clean (optional) -----------------------------------------------------
if [[ "$EXTRA_FLAG" == "clean" ]]; then
    rm -rf "$BUILD_DIR"
    echo "✔ Build directory cleaned"
fi

# ---- configure flags ------------------------------------------------------
case "$BUILD_TYPE" in
    debug)
        run_cmake -DCMAKE_BUILD_TYPE=Debug
        ;;
    full)
        run_cmake -DBUILD_BINDINGS=ON
        ;;
    all)
        run_cmake -DBUILD_BINDINGS=ON \
                  -DENABLE_GNURADIO=ON \
                  -DENABLE_SOAPY=ON \
                  -DCMAKE_PREFIX_PATH="$(pkg-config --variable=prefix gnuradio-runtime 2>/dev/null || echo '')/lib/cmake/gnuradio"
        ;;
    *)
        run_cmake   # release, no optional components
        ;;
esac

# ---- compile ---------------------------------------------------------------
BUILD_CMD="cmake --build \"$BUILD_DIR\""

if [[ "$EXTRA_FLAG" == "parallel" ]]; then
    NPROC=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
    BUILD_CMD="$BUILD_CMD --parallel $NPROC"
    echo "Building with parallelization ($NPROC jobs)..."
else
    echo "Building (single-threaded; use './build.sh <type> parallel' for faster builds)..."
fi

eval "$BUILD_CMD"

# ---- run tests (optional, but always informative) --------------------------
if [[ -x "$BUILD_DIR/unit_tests" ]]; then
    echo ""
    echo "----- unit tests -----"
    "$BUILD_DIR/unit_tests" || true
fi
if [[ -x "$BUILD_DIR/integration_tests" ]]; then
    echo ""
    echo "----- integration tests -----"
    "$BUILD_DIR/integration_tests" || true
fi

# ---- install Python GUI (if bindings were built) --------------------------
if [[ "$BUILD_TYPE" == "full" || "$BUILD_TYPE" == "all" ]]; then
    echo ""
    echo "----- installing GUI package -----"
    cd "$GUI_DIR"
    pip install -e .
    cd ..
    echo "✔ GUI installed (editable)"
fi

echo ""
echo "----- build finished ($BUILD_TYPE) -----"