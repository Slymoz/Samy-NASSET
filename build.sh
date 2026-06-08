#!/bin/bash

set -e

echo "=== CMake Configuration ==="

cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

ln -sf build/compile_commands.json .

echo "=== Compilation ==="

cmake --build build -j$(nproc)

echo ""
echo "Build completed successfully."
echo "Executable : ./build/backtest"
