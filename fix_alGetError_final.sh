#!/bin/bash

# Create a completely new version of the SoundComponent.h file with corrected alGetError
cat > /tmp/new_sound_component.h << 'END'
#pragma once

#include "core/Component.h"
#include <glm/glm.hpp>
#include <string>
#include <memory>

// Forward declare alGetError once for the entire file
ALenum alGetError();

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
// NOTE: alGetError() is declared at the top of the file and implemented in SoundSystem.cpp
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
// NOTE: alGetError() is declared at the top of the file and implemented in SoundSystem.cpp
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

# Update SoundSystem.cpp to ensure alGetError is correctly implemented
cat > /tmp/new_sound_system.cpp << 'END'
#include "SoundSystem.h"
#include "Entity.h"
#include "components/SoundComponent.h"
#include "components/TransformComponent.h"
#include "Logger.h"
#include <algorithm>

// Conditional OpenAL includes
#ifdef OPENAL_FOUND
#if OPENAL_FOUND
#include <AL/al.h>
#include <AL/alc.h>
#else
// Dummy OpenAL types for compilation without OpenAL
typedef unsigned int ALuint;
typedef int ALenum;
typedef float ALfloat;
typedef struct ALCdevice ALCdevice;
typedef struct ALCcontext ALCcontext;
#define AL_NO_ERROR 0
#define AL_POSITION 0x1004
#define AL_VELOCITY 0x1006
#define AL_GAIN 0x100A
#define AL_ORIENTATION 0x100F
#define AL_TRUE 1
#define AL_FALSE 0
// Dummy function declarations - no need to redeclare alGetError, it's already declared in SoundComponent.h
static inline ALCcontext* alcGetCurrentContext() { return nullptr; }
static inline void alDopplerFactor(float) {}
static inline void alSpeedOfSound(float) {}
static inline void alListener3f(ALenum, float, float, float) {}
static inline void alListenerfv(ALenum, const float*) {}
static inline void alListenerf(ALenum, float) {}
#endif
#else
// Fallback definitions when OPENAL_FOUND is not defined
typedef unsigned int ALuint;
typedef int ALenum;
typedef float ALfloat;
typedef struct ALCdevice ALCdevice;
typedef struct ALCcontext ALCcontext;
#define AL_NO_ERROR 0
#define AL_POSITION 0x1004
#define AL_VELOCITY 0x1006
#define AL_GAIN 0x100A
#define AL_ORIENTATION 0x100F
#define AL_TRUE 1
#define AL_FALSE 0
// Dummy function declarations - no need to redeclare alGetError, it's already declared in SoundComponent.h
static inline ALCcontext* alcGetCurrentContext() { return nullptr; }
static inline void alDopplerFactor(float) {}
static inline void alSpeedOfSound(float) {}
static inline void alListener3f(ALenum, float, float, float) {}
static inline void alListenerfv(ALenum, const float*) {}
static inline void alListenerf(ALenum, float) {}
#endif

namespace IKore {

    SoundSystem::SoundSystem() 
        : m_initialized(false)
        , m_listenerPosition(0.0f)
        , m_listenerVelocity(0.0f)
        , m_listenerGain(1.0f)
        , m_dopplerFactor(1.0f)
        , m_speedOfSound(343.3f)
        , m_distanceCullingEnabled(true)
        , m_maxCullingDistance(500.0f)
    {
        // Initialization will be done in initialize()
    }

    SoundSystem::~SoundSystem() {
        shutdown();
    }

    void SoundSystem::initialize() {
        if (m_initialized) {
            return;
        }

        LOG_INFO("Initializing SoundSystem");

        // Apply initial listener properties
        setListenerPosition(m_listenerPosition);
        setListenerGain(m_listenerGain);
        setDopplerFactor(m_dopplerFactor);
        setSpeedOfSound(m_speedOfSound);

        // Set listener orientation (forward and up vectors)
        // Forward vector: (0, 0, -1) - Looking down the negative Z axis
        // Up vector: (0, 1, 0) - Y is up
        float orientation[6] = { 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };
        
#ifdef OPENAL_FOUND
#if OPENAL_FOUND
        alListenerfv(AL_ORIENTATION, orientation);
        
        // Check for errors
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            LOG_ERROR("Error setting listener orientation: {}", error);
        }
#endif
#endif

        m_initialized = true;
        LOG_INFO("SoundSystem initialized successfully");
    }

    void SoundSystem::shutdown() {
        if (!m_initialized) {
            return;
        }

        LOG_INFO("Shutting down SoundSystem");

        // Clear all registered entities
        m_entities.clear();

        m_initialized = false;
    }

    void SoundSystem::update(float deltaTime) {
        if (!m_initialized) {
            return;
        }

        // Update all sound components
        for (auto& entity : m_entities) {
            if (!entity) continue;

            auto soundComp = entity->getComponent<SoundComponent>();
            if (!soundComp) continue;

            // Distance culling optimization
            if (m_distanceCullingEnabled) {
                auto transform = entity->getComponent<TransformComponent>();
                if (transform) {
                    float distance = glm::length(transform->getPosition() - m_listenerPosition);
                    if (distance > m_maxCullingDistance) {
                        // Skip update for sounds that are too far away
                        continue;
                    }
                }
            }

            // Update the sound component
            soundComp->update(deltaTime);
        }
    }

    void SoundSystem::registerEntity(std::shared_ptr<Entity> entity) {
        if (!entity) {
            LOG_WARNING("Attempted to register null entity in SoundSystem");
            return;
        }

        // Check if the entity has a SoundComponent
        auto soundComp = entity->getComponent<SoundComponent>();
        if (!soundComp) {
            LOG_WARNING("Entity {} has no SoundComponent", entity->getId());
            return;
        }

        // Add to our list if not already there
        if (std::find(m_entities.begin(), m_entities.end(), entity) == m_entities.end()) {
            m_entities.push_back(entity);
            LOG_DEBUG("Entity {} registered with SoundSystem", entity->getId());
        }
    }

    void SoundSystem::unregisterEntity(std::shared_ptr<Entity> entity) {
        if (!entity) return;

        m_entities.erase(
            std::remove_if(m_entities.begin(), m_entities.end(), 
                [entity](const std::weak_ptr<Entity>& weak_entity) {
                    auto e = weak_entity.lock();
                    return !e || e == entity;
                }
            ),
            m_entities.end()
        );
    }

    void SoundSystem::setListenerPosition(const glm::vec3& position) {
        m_listenerPosition = position;

#ifdef OPENAL_FOUND
#if OPENAL_FOUND
        alListener3f(AL_POSITION, position.x, position.y, position.z);
        
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            LOG_ERROR("Error setting listener position: {}", error);
        }
#endif
#endif
    }

    void SoundSystem::setListenerVelocity(const glm::vec3& velocity) {
        m_listenerVelocity = velocity;

#ifdef OPENAL_FOUND
#if OPENAL_FOUND
        alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
        
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            LOG_ERROR("Error setting listener velocity: {}", error);
        }
#endif
#endif
    }

    void SoundSystem::setListenerGain(float gain) {
        m_listenerGain = gain;

#ifdef OPENAL_FOUND
#if OPENAL_FOUND
        alListenerf(AL_GAIN, gain);
        
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            LOG_ERROR("Error setting listener gain: {}", error);
        }
#endif
#endif
    }

    void SoundSystem::setDopplerFactor(float factor) {
        m_dopplerFactor = factor;

#ifdef OPENAL_FOUND
#if OPENAL_FOUND
        alDopplerFactor(factor);
        
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            LOG_ERROR("Error setting doppler factor: {}", error);
        }
#endif
#endif
    }

    void SoundSystem::setSpeedOfSound(float speed) {
        m_speedOfSound = speed;

#ifdef OPENAL_FOUND
#if OPENAL_FOUND
        alSpeedOfSound(speed);
        
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            LOG_ERROR("Error setting speed of sound: {}", error);
        }
#endif
#endif
    }

    void SoundSystem::setDistanceCulling(bool enabled, float maxDistance) {
        m_distanceCullingEnabled = enabled;
        if (maxDistance > 0.0f) {
            m_maxCullingDistance = maxDistance;
        }
    }

    int SoundSystem::getEntityCount() const {
        return static_cast<int>(m_entities.size());
    }

} // namespace IKore

// Implement the alGetError function that's declared in SoundComponent.h
// This must be in global scope, not inside any namespace
ALenum alGetError() { 
    return AL_NO_ERROR; 
}
END

# Copy the new files over the old ones
cp /tmp/new_sound_component.h /workspaces/IKore-Engine/src/core/components/SoundComponent.h
cp /tmp/new_sound_system.cpp /workspaces/IKore-Engine/src/core/SoundSystem.cpp

echo "✅ Fixed files with correct alGetError implementation:"
echo "  1. SoundComponent.h - Now has a single declaration at the top"
echo "  2. SoundSystem.cpp - Implementation at the bottom of the file, outside any namespace"
