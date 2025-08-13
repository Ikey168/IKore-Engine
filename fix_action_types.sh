#!/bin/bash
# Fix GLFW constants to InputActionType constants in ControllableEntities.cpp

FILE="/workspaces/IKore-Engine/src/ControllableEntities.cpp"

# Replace GLFW constants with InputActionType constants
sed -i 's/event\.action == GLFW_PRESS/event.action == InputActionType::PRESS/g' "$FILE"
sed -i 's/event\.action == GLFW_RELEASE/event.action == InputActionType::RELEASE/g' "$FILE"  
sed -i 's/event\.action == GLFW_REPEAT/event.action == InputActionType::REPEAT/g' "$FILE"

echo "Fixed GLFW constants to InputActionType in ControllableEntities.cpp"
