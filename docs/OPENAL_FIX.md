# OpenAL and alGetError() CI Build Fix

## Problem Overview

The CI builds for PR #130 are failing on all platforms (ubuntu, windows, macOS) due to issues with OpenAL type definitions and the implementation of `alGetError()`.

### Specific Issues Identified:

1. **Type Redefinition Conflicts:**
   - Both `SoundComponent.h` and `SoundSystem.cpp` define custom OpenAL types (`ALenum`, `ALuint`, etc.) when `OPENAL_FOUND` is not defined
   - These duplicate definitions cause compilation errors when both files are included together

2. **Function Declaration/Definition Issues:**
   - `alGetError()` is forward-declared in `SoundComponent.h` before the `ALenum` type is defined
   - The implementation in `SoundSystem.cpp` conflicts with the forward declaration due to type incompatibilities

3. **`weak_ptr` Usage Issues:**
   - There are several errors related to using `weak_ptr<Entity>` incorrectly in `SoundSystem.cpp`

## Solution

To fix these issues, the following changes have been made:

1. **Fix SoundComponent.h:**
   - Move the forward declaration of `alGetError()` after all type definitions
   - Mark the declaration with `extern` to clarify it's defined elsewhere
   - Ensure consistent type definitions for both `OPENAL_FOUND` and non-OpenAL cases

2. **Fix SoundSystem.cpp:**
   - Remove duplicate type definitions that conflict with those in `SoundComponent.h`
   - Update OpenAL includes to only define types if not already defined
   - Ensure the `alGetError()` implementation matches the declaration

3. **CI Environment Setup:**
   - Ensure proper `.asoundrc` configuration is in place for headless CI environments

## Implementation

The script `fix_openal_declarations.sh` implements these changes automatically. After applying these fixes, the CI builds should succeed.

## Technical Details

- Forward declarations must appear after the types they use are defined
- Multiple typedefs for the same name in different compilation units cause errors
- The `extern` keyword clarifies that a function is defined in another file
- Proper header guards prevent duplicate type definitions

This approach follows C++ best practices for declaration and definition patterns in conditionally compiled code.
