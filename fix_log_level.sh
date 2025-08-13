#!/bin/bash
# Fix Logger::INFO to LogLevel::INFO in ControllableEntities.cpp

FILE="/workspaces/IKore-Engine/src/ControllableEntities.cpp"

# Replace Logger::INFO with LogLevel::INFO
sed -i 's/Logger::INFO/LogLevel::INFO/g' "$FILE"

echo "Fixed Logger::INFO to LogLevel::INFO in ControllableEntities.cpp"
