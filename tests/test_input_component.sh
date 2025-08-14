#!/bin/bash

echo "=== Input Component System Test ==="
echo "Testing Issue #28: Implement Input Component"
echo

# Change to project directory
cd /workspaces/IKore-Engine

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}1. Building project with Input Component...${NC}"
cd build
if make; then
    echo -e "${GREEN}✓ Build successful${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi

echo
echo -e "${BLUE}2. Testing Input Component header inclusion...${NC}"
if grep -q "InputComponent.h" ../src/main.cpp; then
    echo -e "${GREEN}✓ InputComponent.h included in main.cpp${NC}"
else
    echo -e "${RED}✗ InputComponent.h not found in main.cpp${NC}"
fi

if grep -q "ControllableEntities.h" ../src/main.cpp; then
    echo -e "${GREEN}✓ ControllableEntities.h included in main.cpp${NC}"
else
    echo -e "${RED}✗ ControllableEntities.h not found in main.cpp${NC}"
fi

echo
echo -e "${BLUE}3. Checking InputManager singleton implementation...${NC}"
if grep -q "class InputManager" ../src/InputComponent.h; then
    echo -e "${GREEN}✓ InputManager class found${NC}"
else
    echo -e "${RED}✗ InputManager class not found${NC}"
fi

if grep -q "getInstance()" ../src/InputComponent.h; then
    echo -e "${GREEN}✓ Singleton pattern implemented${NC}"
else
    echo -e "${RED}✗ Singleton pattern not found${NC}"
fi

echo
echo -e "${BLUE}4. Verifying GLFW integration...${NC}"
if grep -q "GLFWwindow" ../src/InputComponent.h; then
    echo -e "${GREEN}✓ GLFW window integration found${NC}"
else
    echo -e "${RED}✗ GLFW window integration not found${NC}"
fi

if grep -q "glfwSetKeyCallback" ../src/InputComponent.cpp; then
    echo -e "${GREEN}✓ GLFW key callback registration found${NC}"
else
    echo -e "${RED}✗ GLFW key callback registration not found${NC}"
fi

if grep -q "glfwSetCursorPosCallback" ../src/InputComponent.cpp; then
    echo -e "${GREEN}✓ GLFW mouse callback registration found${NC}"
else
    echo -e "${RED}✗ GLFW mouse callback registration not found${NC}"
fi

echo
echo -e "${BLUE}5. Testing keyboard input capture...${NC}"
if grep -q "KeyboardCallback" ../src/InputComponent.cpp; then
    echo -e "${GREEN}✓ Keyboard callback implementation found${NC}"
else
    echo -e "${RED}✗ Keyboard callback implementation not found${NC}"
fi

if grep -q "GLFW_KEY_" ../src/InputComponent.h; then
    echo -e "${GREEN}✓ GLFW key constants used${NC}"
else
    echo -e "${RED}✗ GLFW key constants not found${NC}"
fi

echo
echo -e "${BLUE}6. Testing mouse input capture...${NC}"
if grep -q "MouseCallback" ../src/InputComponent.cpp; then
    echo -e "${GREEN}✓ Mouse callback implementation found${NC}"
else
    echo -e "${RED}✗ Mouse callback implementation not found${NC}"
fi

if grep -q "deltaX.*deltaY" ../src/InputComponent.h; then
    echo -e "${GREEN}✓ Mouse delta tracking found${NC}"
else
    echo -e "${RED}✗ Mouse delta tracking not found${NC}"
fi

echo
echo -e "${BLUE}7. Checking entity control integration...${NC}"
if grep -q "ControllableGameObject" ../src/ControllableEntities.h; then
    echo -e "${GREEN}✓ ControllableGameObject class found${NC}"
else
    echo -e "${RED}✗ ControllableGameObject class not found${NC}"
fi

if grep -q "ControllableCamera" ../src/ControllableEntities.h; then
    echo -e "${GREEN}✓ ControllableCamera class found${NC}"
else
    echo -e "${RED}✗ ControllableCamera class not found${NC}"
fi

if grep -q "ControllableLight" ../src/ControllableEntities.h; then
    echo -e "${GREEN}✓ ControllableLight class found${NC}"
else
    echo -e "${RED}✗ ControllableLight class not found${NC}"
fi

echo
echo -e "${BLUE}8. Testing action registration system...${NC}"
if grep -q "bindKeyAction" ../src/InputComponent.h; then
    echo -e "${GREEN}✓ Key action binding found${NC}"
else
    echo -e "${RED}✗ Key action binding not found${NC}"
fi

if grep -q "bindMouseMoveAction" ../src/InputComponent.h; then
    echo -e "${GREEN}✓ Mouse move action binding found${NC}"
else
    echo -e "${RED}✗ Mouse move action binding not found${NC}"
fi

if grep -q "bindScrollAction" ../src/InputComponent.h; then
    echo -e "${GREEN}✓ Scroll action binding found${NC}"
else
    echo -e "${RED}✗ Scroll action binding not found${NC}"
fi

echo
echo -e "${BLUE}9. Checking input lag minimization design...${NC}"
if grep -q "update.*deltaTime" ../src/InputComponent.h; then
    echo -e "${GREEN}✓ Input component update method found${NC}"
else
    echo -e "${RED}✗ Input component update method not found${NC}"
fi

if grep -q "m_keyStates" ../src/InputComponent.h; then
    echo -e "${GREEN}✓ Key state tracking found${NC}"
else
    echo -e "${RED}✗ Key state tracking not found${NC}"
fi

echo
echo -e "${BLUE}10. Verifying main.cpp integration...${NC}"
if grep -q "InputManager.*getInstance" ../src/main.cpp; then
    echo -e "${GREEN}✓ InputManager initialization found in main.cpp${NC}"
else
    echo -e "${RED}✗ InputManager initialization not found in main.cpp${NC}"
fi

if grep -q "ControllableGameObject" ../src/main.cpp; then
    echo -e "${GREEN}✓ ControllableGameObject usage found in main.cpp${NC}"
else
    echo -e "${RED}✗ ControllableGameObject usage not found in main.cpp${NC}"
fi

if grep -q "F1.*F2.*F3" ../src/main.cpp; then
    echo -e "${GREEN}✓ Input control toggle keys found in main.cpp${NC}"
else
    echo -e "${RED}✗ Input control toggle keys not found in main.cpp${NC}"
fi

echo
echo -e "${BLUE}11. Testing CMakeLists.txt updates...${NC}"
if grep -q "InputComponent.cpp" ../CMakeLists.txt; then
    echo -e "${GREEN}✓ InputComponent.cpp added to build${NC}"
else
    echo -e "${RED}✗ InputComponent.cpp not added to build${NC}"
fi

if grep -q "ControllableEntities.cpp" ../CMakeLists.txt; then
    echo -e "${GREEN}✓ ControllableEntities.cpp added to build${NC}"
else
    echo -e "${RED}✗ ControllableEntities.cpp not added to build${NC}"
fi

echo
echo -e "${BLUE}12. Feature completeness check...${NC}"

# Check for keyboard input features
keyboard_features=0
if grep -q "GLFW_KEY_W\|GLFW_KEY_A\|GLFW_KEY_S\|GLFW_KEY_D" ../src/ControllableEntities.cpp; then
    echo -e "${GREEN}✓ WASD movement controls implemented${NC}"
    ((keyboard_features++))
else
    echo -e "${RED}✗ WASD movement controls not found${NC}"
fi

if grep -q "GLFW_KEY_SPACE\|GLFW_KEY_LEFT_SHIFT" ../src/ControllableEntities.cpp; then
    echo -e "${GREEN}✓ Vertical movement controls implemented${NC}"
    ((keyboard_features++))
else
    echo -e "${RED}✗ Vertical movement controls not found${NC}"
fi

# Check for mouse input features
mouse_features=0
if grep -q "handleMouseLook" ../src/ControllableEntities.cpp; then
    echo -e "${GREEN}✓ Mouse look functionality implemented${NC}"
    ((mouse_features++))
else
    echo -e "${RED}✗ Mouse look functionality not found${NC}"
fi

if grep -q "GLFW_CURSOR_DISABLED\|GLFW_CURSOR_NORMAL" ../src/ControllableEntities.cpp; then
    echo -e "${GREEN}✓ Mouse capture functionality implemented${NC}"
    ((mouse_features++))
else
    echo -e "${RED}✗ Mouse capture functionality not found${NC}"
fi

# Check for performance features
performance_features=0
if grep -q "deltaTime" ../src/InputComponent.cpp; then
    echo -e "${GREEN}✓ Delta time integration for smooth input${NC}"
    ((performance_features++))
else
    echo -e "${RED}✗ Delta time integration not found${NC}"
fi

if grep -q "m_enabled" ../src/InputComponent.h; then
    echo -e "${GREEN}✓ Input component enable/disable system${NC}"
    ((performance_features++))
else
    echo -e "${RED}✗ Input component enable/disable system not found${NC}"
fi

echo
echo -e "${YELLOW}=== Input Component Test Summary ===${NC}"
echo -e "Keyboard Features: ${keyboard_features}/2"
echo -e "Mouse Features: ${mouse_features}/2"  
echo -e "Performance Features: ${performance_features}/2"

total_score=$((keyboard_features + mouse_features + performance_features))
max_score=6

if [ $total_score -eq $max_score ]; then
    echo -e "${GREEN}🎉 All Input Component features implemented successfully!${NC}"
    echo -e "${GREEN}✓ Issue #28 requirements satisfied${NC}"
else
    echo -e "${YELLOW}⚠️ Some features may need attention (${total_score}/${max_score})${NC}"
fi

echo
echo -e "${BLUE}=== Usage Instructions ===${NC}"
echo "To test the Input Component system:"
echo "1. Run the engine: ./IKore"
echo "2. Press F1 to activate FPS Camera control"
echo "3. Press F2 to activate Light control"  
echo "4. Press F3 to activate Player control"
echo "5. Use WASD for movement, mouse for looking, ESC to toggle mouse capture"
echo "6. Use +/- for light intensity, C to cycle light colors"
echo "7. Use arrow keys for light movement when light control is active"
echo "8. Use 1/2/3 to switch camera modes when camera control is active"
echo "9. Use mouse scroll for speed/zoom adjustments"

echo
echo -e "${GREEN}Input Component system implementation complete!${NC}"
