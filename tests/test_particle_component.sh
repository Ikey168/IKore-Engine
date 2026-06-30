#!/bin/bash

# Build and run the Particle Component test

# Ensure we have the right build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build the test
make test_particle_component

# Run the test
./test_particle_component

# Return to the original directory
cd ..
