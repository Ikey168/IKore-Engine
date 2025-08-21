#!/bin/bash

# Create a pattern file for SoundComponent.h replacements
cat > /tmp/pattern.sed << 'END'
# Replace the function definition with declaration in SoundComponent.h
s/static inline ALenum alGetError() { return AL_NO_ERROR; }/ALenum alGetError(); \/\/ Declaration only - definition moved to avoid redefinition/g
END

# Apply to the SoundComponent.h file (both instances)
sed -i -f /tmp/pattern.sed /workspaces/IKore-Engine/src/core/components/SoundComponent.h

# Now add the definition to SoundSystem.cpp at the end of the namespace
cat > /tmp/implementation.cpp << 'END'
// Implement the alGetError function that's declared in SoundComponent.h
ALenum alGetError() { 
    return AL_NO_ERROR; 
}
END

# Append the implementation to the end of SoundSystem.cpp
cat /tmp/implementation.cpp >> /workspaces/IKore-Engine/src/core/SoundSystem.cpp

echo "✅ Fixed alGetError redefinition issue in both files"
