#!/bin/bash

echo "=== IKore Engine Animation Component Test ==="
echo "Testing AnimationComponent implementation for Issue #81"
echo ""

# Build the project
echo "Building animation test..."
cd /workspaces/IKore-Engine

# Configure and build with CMake  
mkdir -p build_test
cd build_test

cmake .. -DCMAKE_BUILD_TYPE=Debug
if [ $? -ne 0 ]; then
    echo "❌ CMake configuration failed"
    exit 1
fi

make test_animation_component -j$(nproc)
if [ $? -ne 0 ]; then
    echo "❌ Build failed"
    exit 1
fi

echo "✅ Build successful"
echo ""

# Run the test
echo "Running animation component test..."
./test_animation_component

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Animation Component test completed successfully!"
    echo "🎬 AnimationComponent is ready for skeletal animations"
    echo ""
    echo "Features implemented:"
    echo "  • Skeletal animation system with bone transformations"
    echo "  • Animation blending and transitions"
    echo "  • Root motion support"
    echo "  • Animation events and callbacks"
    echo "  • Assimp integration for loading animations"
    echo "  • Multi-layer animation support"
    echo ""
    echo "Issue #81 Animation Component implementation complete! 🎉"
else
    echo ""
    echo "❌ Animation Component test failed"
    exit 1
fi
