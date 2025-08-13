#!/bin/bash
# Replace Logger::getInstance().log calls with LOG_INFO macro

FILE="/workspaces/IKore-Engine/src/ControllableEntities.cpp"

# Replace complex log calls with simple LOG_INFO calls
sed -i 's/Logger::getInstance()\.log(LogLevel::INFO, \([^)]*\))/LOG_INFO(\1)/g' "$FILE"
sed -i 's/Logger::getInstance()\.log(\([^,]*\), LogLevel::INFO)/LOG_INFO(\1)/g' "$FILE"

echo "Replaced Logger calls with LOG_INFO macro in ControllableEntities.cpp"
