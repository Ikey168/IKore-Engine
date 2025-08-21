#!/bin/bash
# This script fixes issues with OpenAL and alGetError declarations to resolve CI build failures

# Fix for SoundComponent.h
cat << 'EOF' > src/core/components/SoundComponent.h.new
#pragma once

#include "core/Component.h"
#include <glm/glm.hpp>
#include <string>
#include <memory>

// Conditional OpenAL includes
#ifdef OPENAL_FOUND
#if OPENAL_FOUND
#include <AL/al.h>
#include <AL/alc.h>
#else
// Dummy OpenAL types for compilation without OpenAL
typedef unsigned int ALuint;
typedef int ALenum;
typedef int ALint;
#endif
#else
// Dummy OpenAL types for compilation without OpenAL
typedef unsigned int ALuint;
typedef int ALenum;
typedef int ALint;
#endif

// Forward declare alGetError once for the entire file
extern ALenum alGetError();
EOF

# Get the rest of the file after the types
tail -n +22 src/core/components/SoundComponent.h >> src/core/components/SoundComponent.h.new
mv src/core/components/SoundComponent.h.new src/core/components/SoundComponent.h

# Fix for SoundSystem.cpp
sed -i '1,25s/typedef int ALenum;//g' src/core/SoundSystem.cpp
sed -i '1,25s/typedef unsigned int ALuint;//g' src/core/SoundSystem.cpp
sed -i '1,25s/\// Conditional OpenAL includes/\// Conditional OpenAL includes - but only if needed/g' src/core/SoundSystem.cpp

echo "Fixes applied successfully!"
echo ""
echo "The changes modify:"
echo "1. SoundComponent.h - Moved forward declaration after type definitions"
echo "2. SoundSystem.cpp - Removed duplicate typedefs that conflict with SoundComponent.h"
echo ""
echo "These changes should fix the CI build failures related to OpenAL typedefs and alGetError declaration."
