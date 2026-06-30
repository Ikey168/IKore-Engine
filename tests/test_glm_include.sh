#!/bin/bash

# This is a test script to verify that glm includes are properly handled

# Ensure build directory exists
mkdir -p build_test

# Compile the simple test with explicit glm include path
echo "Compiling with explicit GLM include..."
g++ -std=c++17 -I. -o build_test/test_glm_include test_event_system_simple.cpp src/core/EventSystem.cpp -pthread -I/usr/include/glm

if [ $? -eq 0 ]; then
    echo "Compilation with explicit GLM include successful!"
else
    echo "Compilation with explicit GLM include failed!"
fi

# Compile without explicit glm include to see if our CMake changes worked
echo "Compiling without explicit GLM include..."
g++ -std=c++17 -I. -o build_test/test_no_glm_include test_event_system_simple.cpp src/core/EventSystem.cpp -pthread

if [ $? -eq 0 ]; then
    echo "Compilation without explicit GLM include successful! The issue is fixed."
else
    echo "Compilation without explicit GLM include failed! We still need to fix the include paths."
fi
