#!/bin/bash

# Create a pattern file for SoundSystem.cpp replacements
cat > /tmp/system_pattern.sed << 'END'
# Replace the inline definition with just a declaration
s/static inline ALenum alGetError() { return AL_NO_ERROR; }/ALenum alGetError(); \/\/ Declaration only - implementation at end of file/g
END

# Apply to the SoundSystem.cpp file
sed -i -f /tmp/system_pattern.sed /workspaces/IKore-Engine/src/core/SoundSystem.cpp

echo "✅ Fixed alGetError redefinition in SoundSystem.cpp"
