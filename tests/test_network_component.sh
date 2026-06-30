#!/bin/bash
# Build script for the NetworkComponent test

# Make sure we're in the right directory
cd "$(dirname "$0")"

# Create build directory if it doesn't exist
mkdir -p build_test
cd build_test

# Run CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build the test
cmake --build . --target test_network_component -j$(nproc)

# Check if the build was successful
if [ $? -eq 0 ]; then
    echo "Build successful! Running test..."
    ./test_network_component
else
    echo "Build failed!"
    exit 1
fi
