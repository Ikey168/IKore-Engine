#!/bin/bash
set -e

echo "=== Cleaning previous build files ==="
rm -rf build_ci
mkdir -p build_ci
cd build_ci

echo "=== Running CMake ==="
cmake .. -DCMAKE_BUILD_TYPE=Release

echo "=== Building project ==="
cmake --build . -j$(nproc)

echo "=== Building successful! ==="
cd ..

echo "All done! If you see this message, the build was successful."
