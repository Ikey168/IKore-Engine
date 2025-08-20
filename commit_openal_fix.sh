#!/bin/bash
# Script to commit the OpenAL fixes

# Check if we're in the right directory
if [ ! -d ".git" ]; then
  echo "Error: Not in a git repository root. Please run from the IKore-Engine directory."
  exit 1
fi

# Add the modified files
git add src/core/SoundSystem.cpp
git add OPENAL_FIX.md

# Commit the changes
git commit -m "Fix OpenAL declarations to resolve CI build errors

- Removed duplicate typedefs for ALuint and ALenum in SoundSystem.cpp
- These types are already defined in SoundComponent.h
- Added comments to clarify OpenAL type usage
- Added documentation explaining the issues and solution

See OPENAL_FIX.md for more details."

# Show commit status
echo "Commit created successfully. You can now push with:"
echo "git push origin 82-sound-component"
