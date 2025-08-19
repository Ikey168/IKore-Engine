#!/bin/bash

# Create a temporary file with the fixed content
cat > /tmp/SoundComponent.h << 'END'
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
typedef float ALfloat;
typedef int ALsizei;
typedef char ALCchar;
typedef char ALCboolean;
typedef int ALCint;
typedef struct ALCdevice ALCdevice;
typedef struct ALCcontext ALCcontext;
#define AL_NO_ERROR 0
#define AL_INITIAL 0x1011
#define AL_PLAYING 0x1012
#define AL_PAUSED 0x1013
#define AL_STOPPED 0x1014
#define AL_FALSE 0
#define AL_TRUE 1
#define AL_FORMAT_MONO16 0x1101
#define AL_BUFFER 0x1009
#define AL_POSITION 0x1004
#define AL_GAIN 0x100A
#define AL_PITCH 0x1003
#define AL_LOOPING 0x1007
#define AL_REFERENCE_DISTANCE 0x1020
#define AL_ROLLOFF_FACTOR 0x1021
#define AL_MAX_DISTANCE 0x1023
#define AL_MIN_GAIN 0x100D
#define AL_MAX_GAIN 0x100E

// Fallback function declarations when OpenAL is not available
static inline void alDeleteBuffers(ALuint, ALuint*) {}
static inline void alGenBuffers(ALuint, ALuint*) {}
static inline void alBufferData(ALuint, ALenum, const void*, ALsizei, ALsizei) {}
static inline void alSourcei(ALuint, ALenum, ALint) {}
static inline void alGenSources(ALuint, ALuint*) {}
static inline void alDeleteSources(ALuint, ALuint*) {}
static inline void alSourcePlay(ALuint) {}
static inline void alSourcePause(ALuint) {}
static inline void alSourceStop(ALuint) {}
static inline void alSource3f(ALuint, ALenum, ALfloat, ALfloat, ALfloat) {}
static inline void alSourcef(ALuint, ALenum, ALfloat) {}
static inline void alGetSourcei(ALuint, ALenum, ALint*) {}
static inline ALenum alGetError() { return AL_NO_ERROR; }
static inline ALCdevice* alcOpenDevice(const ALCchar*) { return nullptr; }
static inline ALCcontext* alcCreateContext(ALCdevice*, const ALCint*) { return nullptr; }
static inline ALCboolean alcMakeContextCurrent(ALCcontext*) { return AL_FALSE; }
static inline void alcDestroyContext(ALCcontext*) {}
static inline void alcCloseDevice(ALCdevice*) {}
#endif
#else
// Fallback definitions when OPENAL_FOUND is not defined
typedef unsigned int ALuint;
typedef int ALenum;
typedef int ALint;
typedef float ALfloat;
typedef int ALsizei;
typedef char ALCchar;
typedef char ALCboolean;
typedef int ALCint;
typedef struct ALCdevice ALCdevice;
typedef struct ALCcontext ALCcontext;
#define AL_NO_ERROR 0
#define AL_INITIAL 0x1011
#define AL_PLAYING 0x1012
#define AL_PAUSED 0x1013
#define AL_STOPPED 0x1014
#define AL_FALSE 0
#define AL_TRUE 1
#define AL_FORMAT_MONO16 0x1101
#define AL_BUFFER 0x1009
#define AL_POSITION 0x1004
#define AL_GAIN 0x100A
#define AL_PITCH 0x1003
#define AL_LOOPING 0x1007
#define AL_REFERENCE_DISTANCE 0x1020
#define AL_ROLLOFF_FACTOR 0x1021
#define AL_MAX_DISTANCE 0x1023
#define AL_MIN_GAIN 0x100D
#define AL_MAX_GAIN 0x100E

// Fallback function declarations when OPENAL_FOUND is not defined
static inline void alDeleteBuffers(ALuint, ALuint*) {}
static inline void alGenBuffers(ALuint, ALuint*) {}
static inline void alBufferData(ALuint, ALenum, const void*, ALsizei, ALsizei) {}
static inline void alSourcei(ALuint, ALenum, ALint) {}
static inline void alGenSources(ALuint, ALuint*) {}
static inline void alDeleteSources(ALuint, ALuint*) {}
static inline void alSourcePlay(ALuint) {}
static inline void alSourcePause(ALuint) {}
static inline void alSourceStop(ALuint) {}
static inline void alSource3f(ALuint, ALenum, ALfloat, ALfloat, ALfloat) {}
static inline void alSourcef(ALuint, ALenum, ALfloat) {}
static inline void alGetSourcei(ALuint, ALenum, ALint*) {}
static inline ALenum alGetError() { return AL_NO_ERROR; }
static inline ALCdevice* alcOpenDevice(const ALCchar*) { return nullptr; }
static inline ALCcontext* alcCreateContext(ALCdevice*, const ALCint*) { return nullptr; }
static inline ALCboolean alcMakeContextCurrent(ALCcontext*) { return AL_FALSE; }
static inline void alcDestroyContext(ALCcontext*) {}
static inline void alcCloseDevice(ALCdevice*) {}
#endif

namespace IKore {

    /**
     * @brief Simple Sound Component for 3D positional audio
     * 
     * This component provides a streamlined interface for attaching sound
     * to entities with 3D positional audio support using OpenAL.
     * 
     * Features:
     * - Automatic 3D positioning based on entity transform
     * - Distance-based volume attenuation  
     * - Simple play, pause, stop controls
     * - Optimized for real-time usage
     * - Lightweight and focused on core functionality
     * 
     * Usage:
     * @code
     * auto entity = std::make_shared<Entity>();
     * auto soundComp = entity->addComponent<SoundComponent>();
     * 
     * soundComp->loadSound("footstep.wav");
     * soundComp->play();
     * soundComp->setVolume(0.8f);
     * @endcode
     */
    class SoundComponent : public Component {
    public:
        SoundComponent();
        ~SoundComponent();

        // Overridden from Component base class
        void initialize() override;
        void update(float deltaTime) override;
        void render() override {}  // Nothing to render
        void shutdown() override;
        
        // Sound loading and basic operations
        bool loadSound(const std::string& filename);
        void play();
        void pause();
        void stop();
        bool isPlaying() const;

        // Sound properties
        void setVolume(float volume);
        float getVolume() const { return m_volume; }
        
        void setPitch(float pitch);
        float getPitch() const { return m_pitch; }
        
        void setGain(float gain);
        float getGain() const { return m_gain; }
        
        void setLooping(bool looping);
        bool isLooping() const { return m_looping; }

        // 3D sound properties
        void setPosition(const glm::vec3& position);
        void updatePositionFromTransform();
        
        void setMaxDistance(float distance);
        float getMaxDistance() const { return m_maxDistance; }
        
        void setRolloffFactor(float factor);
        float getRolloffFactor() const { return m_rolloffFactor; }
        
        void setReferenceDistance(float distance);
        float getReferenceDistance() const { return m_referenceDistance; }
        
        void setMinGain(float gain);
        float getMinGain() const { return m_minGain; }
        
        void setMaxGain(float gain);
        float getMaxGain() const { return m_maxGain; }
        
        // Status information
        bool isInFallbackMode() const { return s_fallbackModeDetected; }
        bool isSoundLoaded() const { return m_isLoaded; }
        const std::string& getFilename() const { return m_filename; }

    private:
        // OpenAL objects
        ALuint m_source;
        ALuint m_buffer;
        
        // Sound properties
        std::string m_filename;
        float m_volume;
        float m_pitch;
        float m_gain;
        bool m_looping;
        bool m_isLoaded;
        
        // 3D audio properties
        float m_maxDistance;
        float m_rolloffFactor;
        float m_referenceDistance;
        float m_minGain;
        float m_maxGain;
        
        // Utility functions
        bool loadWavFile(const std::string& filename, ALuint buffer);
        void applyProperties();
        
        // Static OpenAL management
        static bool s_openALInitialized;
        static bool s_fallbackModeDetected;
        static ALCdevice* s_device;
        static ALCcontext* s_context;
        static int s_componentCount;
        
        static bool initializeOpenAL();
        static void shutdownOpenAL();
    };

}  // namespace IKore
END

# Copy the temporary file to replace the original
cp /tmp/SoundComponent.h /workspaces/IKore-Engine/src/core/components/SoundComponent.h
echo "✅ SoundComponent.h updated with fixes"

# Now also check for issues in the SoundSystem.h
grep -q "AL_TRUE" /workspaces/IKore-Engine/src/core/SoundSystem.h || \
echo "⚠️ SoundSystem.h might need similar fixes (no AL_TRUE found)"
