#!/bin/bash

echo "=== IKore Engine Sound Component Test ==="
echo "Testing SoundComponent implementation for Issue #82"
echo ""

# Build the project
echo "Building sound component test..."
cd /workspaces/IKore-Engine

# Configure and build with CMake  
mkdir -p build_test
cd build_test

cmake .. -DCMAKE_BUILD_TYPE=Debug
if [ $? -ne 0 ]; then
    echo "❌ CMake configuration failed"
    exit 1
fi

make test_sound_component -j$(nproc)
if [ $? -ne 0 ]; then
    echo "❌ Build failed"
    exit 1
fi

echo "✅ Build successful"
echo ""

# Run the test
echo "Running sound component test..."
./test_sound_component

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Sound Component test completed successfully!"
    echo "🔊 SoundComponent is ready for 3D positional audio"
    echo ""
    echo "Features implemented:"
    echo "  • 3D positional audio using OpenAL"
    echo "  • Distance-based volume attenuation"
    echo "  • Real-time performance optimization"
    echo "  • Entity transform synchronization"
    echo "  • Play, pause, stop audio controls"
    echo "  • Comprehensive audio property management"
    echo "  • SoundSystem for centralized management"
    echo ""
    echo "Issue #82 Sound Component implementation complete! 🎉"
else
    echo ""
    echo "❌ Sound Component test failed"
    exit 1
fi
