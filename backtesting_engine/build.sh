#!/usr/bin/env bash
# Configure, build and test the engine.
#
#   ./build.sh            Release build + unit tests
#   ./build.sh Debug      Debug build + unit tests
#   ./build.sh --no-tests Release build only (no GoogleTest download)
set -Eeuo pipefail
cd "$(dirname "$0")"

BUILD_TYPE="Release"
BUILD_TESTS="ON"
for arg in "$@"; do
    case "$arg" in
        Release | Debug | RelWithDebInfo) BUILD_TYPE="$arg" ;;
        --no-tests) BUILD_TESTS="OFF" ;;
        -h | --help)
            sed -n '2,7p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            exit 2
            ;;
    esac
done

JOBS="$(nproc 2>/dev/null || echo 4)"

echo "=== CMake configuration (${BUILD_TYPE}) ==="
cmake -S . -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DBACKTEST_BUILD_TESTS="$BUILD_TESTS"

# Lets editors and clang-tidy find the compile flags from the project root.
ln -sf build/compile_commands.json .

echo "=== Compilation ==="
cmake --build build -j "$JOBS"

if [[ "$BUILD_TESTS" == "ON" ]]; then
    echo "=== Unit tests ==="
    (cd build && ctest --output-on-failure -j "$JOBS")
fi

echo ""
echo "Build completed successfully."
echo "Executable : ./build/backtest"
