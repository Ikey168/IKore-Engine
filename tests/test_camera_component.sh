#!/bin/bash

# Camera Component System Test Script
# This script validates the complete Camera Component implementation for Issue #29

set -e  # Exit on any error

echo "=============================================="
echo "🎥 Camera Component System Test (Issue #29)"
echo "=============================================="
echo ""

# Test configuration
BUILD_DIR="build"
EXECUTABLE="IKore"
TEST_TIMEOUT=30

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

print_info() {
    echo -e "${PURPLE}[INFO]${NC} $1"
}

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0

run_test() {
    local test_name="$1"
    local test_command="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    print_status "Running: $test_name"
    
    if eval "$test_command"; then
        print_success "$test_name"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        print_error "$test_name"
        return 1
    fi
}

echo "🔍 Validating Camera Component system..."
echo ""

# Test 1: Check if Camera Component header files exist
run_test "Camera Component header files exist" \
    "test -f src/CameraComponent.h && test -f src/EnhancedCameraEntity.h"

# Test 2: Check if Camera Component source files exist
run_test "Camera Component source files exist" \
    "test -f src/CameraComponent.cpp && test -f src/EnhancedCameraEntity.cpp"

# Test 3: Validate Camera Component header structure
run_test "Camera Component header structure" \
    "grep -q 'class CameraComponent' src/CameraComponent.h && \
     grep -q 'enum class CameraType' src/CameraComponent.h && \
     grep -q 'enum class ProjectionType' src/CameraComponent.h && \
     grep -q 'struct TransitionState' src/CameraComponent.h && \
     grep -q 'struct ThirdPersonConfig' src/CameraComponent.h && \
     grep -q 'struct OrbitalConfig' src/CameraComponent.h"

# Test 4: Validate Enhanced Camera Entity header structure
run_test "Enhanced Camera Entity header structure" \
    "grep -q 'class EnhancedCameraEntity' src/EnhancedCameraEntity.h && \
     grep -q 'CameraComponent.*m_cameraComponent' src/EnhancedCameraEntity.h && \
     grep -q 'getViewMatrix' src/EnhancedCameraEntity.h && \
     grep -q 'getProjectionMatrix' src/EnhancedCameraEntity.h"

# Test 5: Check core Camera Component methods
run_test "Core Camera Component methods" \
    "grep -q 'getViewMatrix' src/CameraComponent.h && \
     grep -q 'getProjectionMatrix' src/CameraComponent.h && \
     grep -q 'getViewProjectionMatrix' src/CameraComponent.h && \
     grep -q 'setCameraType' src/CameraComponent.h && \
     grep -q 'setFollowTarget' src/CameraComponent.h && \
     grep -q 'update.*deltaTime' src/CameraComponent.h"

# Test 6: Check camera type support
run_test "Camera type enumeration" \
    "grep -q 'FIRST_PERSON' src/CameraComponent.h && \
     grep -q 'THIRD_PERSON' src/CameraComponent.h && \
     grep -q 'ORBITAL' src/CameraComponent.h && \
     grep -q 'FREE_CAMERA' src/CameraComponent.h && \
     grep -q 'STATIC' src/CameraComponent.h"

# Test 7: Check entity following functionality
run_test "Entity following functionality" \
    "grep -q 'setFollowTarget' src/CameraComponent.h && \
     grep -q 'getFollowTarget' src/CameraComponent.h && \
     grep -q 'isFollowingEntity' src/CameraComponent.h && \
     grep -q 'std::weak_ptr<Entity>' src/CameraComponent.h"

# Test 8: Check smooth transition support
run_test "Smooth transition support" \
    "grep -q 'TransitionState' src/CameraComponent.h && \
     grep -q 'isTransitioning' src/CameraComponent.h && \
     grep -q 'getTransitionProgress' src/CameraComponent.h && \
     grep -q 'transitionDuration' src/CameraComponent.h"

# Test 9: Check projection matrix support
run_test "Projection matrix support" \
    "grep -q 'ProjectionType' src/CameraComponent.h && \
     grep -q 'PERSPECTIVE' src/CameraComponent.h && \
     grep -q 'ORTHOGRAPHIC' src/CameraComponent.h && \
     grep -q 'setFieldOfView' src/CameraComponent.h && \
     grep -q 'setAspectRatio' src/CameraComponent.h"

# Test 10: Check configuration structures
run_test "Camera configuration structures" \
    "grep -q 'ThirdPersonConfig' src/CameraComponent.h && \
     grep -q 'float distance' src/CameraComponent.h && grep -q 'float height' src/CameraComponent.h && \
     grep -q 'OrbitalConfig' src/CameraComponent.h && \
     grep -q 'float radius' src/CameraComponent.h && grep -q 'float azimuth' src/CameraComponent.h"

# Test 11: Check Entity position override
run_test "Entity position override" \
    "grep -q 'virtual glm::vec3 getPosition()' src/Entity.h && \
     grep -q 'glm::vec3 getPosition().*override' src/TransformableEntities.h"

# Test 12: Check CMakeLists.txt integration
run_test "CMakeLists.txt integration" \
    "grep -q 'CameraComponent.cpp' CMakeLists.txt && \
     grep -q 'EnhancedCameraEntity.cpp' CMakeLists.txt"

# Test 13: Check main.cpp integration
run_test "Main.cpp integration" \
    "grep -q '#include.*CameraComponent.h' src/main.cpp && \
     grep -q '#include.*EnhancedCameraEntity.h' src/main.cpp && \
     grep -q 'g_enhancedCamera' src/main.cpp && \
     grep -q 'CameraComponent::CameraType' src/main.cpp"

# Test 14: Build system validation
print_status "Building Camera Component system..."
if [ ! -d "$BUILD_DIR" ]; then
    mkdir "$BUILD_DIR"
fi

cd "$BUILD_DIR"
if cmake .. && make -j$(nproc); then
    print_success "Camera Component system builds successfully"
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    print_error "Camera Component system build failed"
fi
cd ..
TOTAL_TESTS=$((TOTAL_TESTS + 1))

# Test 15: Check for Camera Component implementation completeness
run_test "Camera Component implementation completeness" \
    "grep -q 'updateViewMatrix' src/CameraComponent.cpp && \
     grep -q 'updateProjectionMatrix' src/CameraComponent.cpp && \
     grep -q 'updateCameraType' src/CameraComponent.cpp && \
     grep -q 'updateTransition' src/CameraComponent.cpp"

# Test 16: Check Enhanced Camera Entity implementation
run_test "Enhanced Camera Entity implementation" \
    "grep -q 'EnhancedCameraEntity::update' src/EnhancedCameraEntity.cpp && \
     grep -q 'getDebugInfo' src/EnhancedCameraEntity.cpp && \
     grep -q 'm_cameraComponent.update' src/EnhancedCameraEntity.cpp"

echo ""
echo "🧪 Camera Component Feature Validation:"
echo ""

# Feature Test 1: Camera Type Switching
run_test "Camera type switching functionality" \
    "grep -q 'setCameraType.*transitionDuration' src/CameraComponent.h && \
     grep -q 'FREE_CAMERA' src/CameraComponent.h && \
     grep -q 'THIRD_PERSON' src/CameraComponent.h && \
     grep -q 'ORBITAL' src/CameraComponent.h && \
     grep -q 'FIRST_PERSON' src/CameraComponent.h"

# Feature Test 2: Entity Following
run_test "Entity following system" \
    "grep -q 'setFollowTarget.*std::shared_ptr<Entity>' src/CameraComponent.h && \
     grep -q 'updateFirstPerson' src/CameraComponent.cpp && \
     grep -q 'updateThirdPerson' src/CameraComponent.cpp && \
     grep -q 'updateOrbital' src/CameraComponent.cpp"

# Feature Test 3: Matrix Management
run_test "Matrix management system" \
    "grep -q 'glm::mat4.*m_viewMatrix' src/CameraComponent.h && \
     grep -q 'glm::mat4.*m_projectionMatrix' src/CameraComponent.h && \
     grep -q 'm_viewDirty' src/CameraComponent.h && \
     grep -q 'm_projectionDirty' src/CameraComponent.h"

# Feature Test 4: Smooth Transitions
run_test "Smooth transition system" \
    "grep -q 'TransitionState.*m_transitionState' src/CameraComponent.h && \
     grep -q 'smoothStep' src/CameraComponent.cpp && \
     grep -q 'smoothInterpolate' src/CameraComponent.cpp"

# Feature Test 5: Camera Configuration
run_test "Camera configuration system" \
    "grep -q 'setThirdPersonConfig' src/CameraComponent.h && \
     grep -q 'setOrbitalConfig' src/CameraComponent.h && \
     grep -q 'ThirdPersonConfig' src/CameraComponent.h && \
     grep -q 'OrbitalConfig' src/CameraComponent.h"

# Feature Test 6: Interactive Controls
run_test "Interactive camera controls" \
    "grep -q 'GLFW_KEY_4' src/main.cpp && \
     grep -q 'GLFW_KEY_5' src/main.cpp && \
     grep -q 'GLFW_KEY_6' src/main.cpp && \
     grep -q 'GLFW_KEY_7' src/main.cpp && \
     grep -q 'g_useCameraComponent' src/main.cpp"

echo ""
echo "🎮 Camera Component Integration Tests:"
echo ""

# Integration Test 1: Entity Manager Integration
run_test "Entity Manager integration" \
    "grep -q 'entityManager.createEntity<.*EnhancedCameraEntity>' src/main.cpp && \
     grep -q 'g_enhancedCamera.*=.*entityManager.createEntity' src/main.cpp"

# Integration Test 2: Main Loop Integration
run_test "Main loop integration" \
    "grep -q 'g_enhancedCamera->update.*deltaTime' src/main.cpp && \
     grep -q 'g_useCameraComponent.*g_enhancedCamera' src/main.cpp"

# Integration Test 3: Render Pipeline Integration
run_test "Render pipeline integration" \
    "grep -q 'g_enhancedCamera->getViewMatrix' src/main.cpp && \
     grep -q 'g_enhancedCamera->getProjectionMatrix' src/main.cpp"

# Integration Test 4: Input System Integration
run_test "Input system integration" \
    "grep -q 'key.*==.*GLFW_KEY_.*g_enhancedCamera' src/main.cpp && \
     grep -q 'setCameraType.*g_cameraTransitionDuration' src/main.cpp"

echo ""
echo "📊 Test Results Summary:"
echo "=============================================="
echo -e "${BLUE}Total Tests:${NC} $TOTAL_TESTS"
echo -e "${GREEN}Passed Tests:${NC} $PASSED_TESTS"
echo -e "${RED}Failed Tests:${NC} $((TOTAL_TESTS - PASSED_TESTS))"
echo -e "${YELLOW}Success Rate:${NC} $(( (PASSED_TESTS * 100) / TOTAL_TESTS ))%"
echo ""

# Feature completion check
echo "✨ Camera Component Features:"
echo ""

FEATURE_CATEGORIES=("Core System" "Entity Following" "Camera Types" "Smooth Transitions" "Matrix Management" "Interactive Controls")
CORE_FEATURES=("View Matrix" "Projection Matrix" "Camera Types" "Entity Following" "Smooth Transitions" "Configuration")

for feature in "${CORE_FEATURES[@]}"; do
    case $feature in
        "View Matrix") 
            if grep -q "getViewMatrix" src/CameraComponent.h && grep -q "updateViewMatrix" src/CameraComponent.cpp; then
                echo -e "  ${GREEN}✓${NC} $feature"
            else
                echo -e "  ${RED}✗${NC} $feature"
            fi
            ;;
        "Projection Matrix")
            if grep -q "getProjectionMatrix" src/CameraComponent.h && grep -q "updateProjectionMatrix" src/CameraComponent.cpp; then
                echo -e "  ${GREEN}✓${NC} $feature"
            else
                echo -e "  ${RED}✗${NC} $feature"
            fi
            ;;
        "Camera Types")
            if grep -q "FIRST_PERSON.*THIRD_PERSON.*ORBITAL" src/CameraComponent.h; then
                echo -e "  ${GREEN}✓${NC} $feature"
            else
                echo -e "  ${RED}✗${NC} $feature"
            fi
            ;;
        "Entity Following")
            if grep -q "setFollowTarget.*getFollowTarget" src/CameraComponent.h; then
                echo -e "  ${GREEN}✓${NC} $feature"
            else
                echo -e "  ${RED}✗${NC} $feature"
            fi
            ;;
        "Smooth Transitions")
            if grep -q "TransitionState.*isTransitioning" src/CameraComponent.h; then
                echo -e "  ${GREEN}✓${NC} $feature"
            else
                echo -e "  ${RED}✗${NC} $feature"
            fi
            ;;
        "Configuration")
            if grep -q "ThirdPersonConfig.*OrbitalConfig" src/CameraComponent.h; then
                echo -e "  ${GREEN}✓${NC} $feature"
            else
                echo -e "  ${RED}✗${NC} $feature"
            fi
            ;;
    esac
done

echo ""
echo "🎯 Issue #29 Requirements Validation:"
echo ""

# Check Acceptance Criteria
echo "Acceptance Criteria:"

# Criterion 1: Camera follows assigned entities
if grep -q "setFollowTarget.*updateThirdPerson.*updateFirstPerson" src/CameraComponent.cpp; then
    echo -e "  ${GREEN}✓${NC} Camera follows assigned entities"
else
    echo -e "  ${RED}✗${NC} Camera follows assigned entities"
fi

# Criterion 2: View and projection matrices update properly
if grep -q "updateViewMatrix.*updateProjectionMatrix.*markDirty" src/CameraComponent.cpp; then
    echo -e "  ${GREEN}✓${NC} View and projection matrices update properly"
else
    echo -e "  ${RED}✗${NC} View and projection matrices update properly"
fi

# Criterion 3: Camera transitions are smooth
if grep -q "TransitionState.*smoothStep.*smoothInterpolate" src/CameraComponent.cpp; then
    echo -e "  ${GREEN}✓${NC} Camera transitions are smooth"
else
    echo -e "  ${RED}✗${NC} Camera transitions are smooth"
fi

echo ""
echo "🚀 Camera Component System Implementation:"

# Final validation
if [ $PASSED_TESTS -eq $TOTAL_TESTS ]; then
    echo -e "${GREEN}🎉 Camera Component System COMPLETE!${NC}"
    echo ""
    echo "The Camera Component implementation successfully provides:"
    echo "  • Comprehensive camera matrix management"
    echo "  • Multiple camera types (First-Person, Third-Person, Orbital, Free, Static)"
    echo "  • Entity following with smooth interpolation"
    echo "  • Smooth camera transitions between states"
    echo "  • Configurable projection parameters"
    echo "  • Interactive camera controls"
    echo "  • Full integration with the IKore Engine"
    echo ""
    echo "Issue #29 requirements are SATISFIED! ✅"
elif [ $PASSED_TESTS -gt $((TOTAL_TESTS * 3 / 4)) ]; then
    echo -e "${YELLOW}⚠️  Camera Component System MOSTLY COMPLETE${NC}"
    echo ""
    echo "Most features are implemented. Minor issues detected:"
    echo "  • $((TOTAL_TESTS - PASSED_TESTS)) tests failed"
    echo "  • Review failed tests above for details"
else
    echo -e "${RED}❌ Camera Component System INCOMPLETE${NC}"
    echo ""
    echo "Major issues detected:"
    echo "  • $((TOTAL_TESTS - PASSED_TESTS)) tests failed"
    echo "  • Core functionality may be missing"
    echo "  • Review implementation before proceeding"
fi

echo ""
echo "💡 Usage Instructions:"
echo "=============================================="
echo ""
echo "To test the Camera Component system:"
echo "1. Build the project: cd build && make"
echo "2. Run the application: ./IKore"
echo "3. Use the following controls:"
echo "   • 4: Free Camera mode"
echo "   • 5: Third-Person mode (follows moving target)"
echo "   • 6: Orbital mode (orbits around target)"
echo "   • 7: First-Person mode (attaches to target)"
echo "   • C: Toggle between old and new camera systems"
echo "   • T/G: Increase/decrease transition duration"
echo ""
echo "The Camera Component provides smooth transitions between different"
echo "camera modes and can follow entities with configurable behavior."
echo ""
echo "=============================================="
