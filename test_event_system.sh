#!/bin/bash

# Ensure build directory exists
mkdir -p build_test

# Compile the event system test
g++ -std=c++17 -I. -o build_test/test_event_system test_event_system.cpp src/core/EventSystem.cpp src/core/Entity.cpp -pthread

if [ $? -eq 0 ]; then
    echo "Compilation successful. Running test..."
    ./build_test/test_event_system
else
    echo "Compilation failed."
    exit 1
fi
