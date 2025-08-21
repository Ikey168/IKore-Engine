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
