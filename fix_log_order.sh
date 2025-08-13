#!/bin/bash
# Fix Logger parameter order in ControllableEntities.cpp

FILE="/workspaces/IKore-Engine/src/ControllableEntities.cpp"

# Fix the parameter order for all log calls
# Pattern: Logger::getInstance().log("message", LogLevel::INFO);
# Should be: Logger::getInstance().log(LogLevel::INFO, "message");

sed -i 's/Logger::getInstance()\.log(\(".*"\), LogLevel::INFO)/Logger::getInstance().log(LogLevel::INFO, \1)/g' "$FILE"

echo "Fixed Logger parameter order in ControllableEntities.cpp"
