#!/bin/bash
# Fix Logger::Level::INFO to Logger::INFO in ControllableEntities.cpp

FILE="/workspaces/IKore-Engine/src/ControllableEntities.cpp"

# Use sed to replace all occurrences
sed -i 's/Logger::Level::INFO/Logger::INFO/g' "$FILE"

echo "Fixed Logger level issues in ControllableEntities.cpp"
