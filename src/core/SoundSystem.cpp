#include "SoundSystem.h"
#include "Entity.h"
#include "components/SoundComponent.h"
#include "components/TransformComponent.h"
#include "Logger.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <algorithm>

namespace IKore {

    SoundSystem::SoundSystem()
        : m_initialized(false)
        , m_lastUpdateTime(0.0f)
        , m_updateFrequency(60.0f)
        , m_listenerPosition(0.0f)
        , m_listenerVelocity(0.0f)
        , m_listenerForward(0.0f, 0.0f, -1.0f)
        , m_listenerUp(0.0f, 1.0f, 0.0f)
        , m_globalVolume(1.0f)
        , m_dopplerFactor(1.0f)
        , m_speedOfSound(343.3f)
        , m_distanceCullingEnabled(true)
        , m_maxCullingDistance(1000.0f)
        , m_maxActiveComponents(64) {
    }

    SoundSystem::~SoundSystem() {
        shutdown();
    }

    bool SoundSystem::initialize() {
        if (m_initialized) {
            Logger::getInstance().warning("SoundSystem already initialized");
            return true;
        }

        // OpenAL is initialized by SoundComponents themselves
        // Here we just set up the listener
        updateListener();
        
        // Set global audio properties
        alDopplerFactor(m_dopplerFactor);
        alSpeedOfSound(m_speedOfSound);
        
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            Logger::getInstance().error("Failed to initialize SoundSystem: " + std::to_string(error));
            return false;
        }

        m_initialized = true;
        Logger::getInstance().info("SoundSystem initialized successfully");
        return true;
    }

    void SoundSystem::shutdown() {
        if (!m_initialized) return;

        clear();
        m_initialized = false;
        Logger::getInstance().info("SoundSystem shut down");
    }

    void SoundSystem::update(float deltaTime) {
        if (!m_initialized) return;

        m_lastUpdateTime += deltaTime;
        
        // Update at specified frequency for performance
        if (m_lastUpdateTime < (1.0f / m_updateFrequency)) {
            return;
        }

        // Clean up invalid entities
        cleanupInvalidEntities();
        
        // Update listener
        updateListener();
        
        // Update active components
        updateComponents(m_lastUpdateTime);
        
        // Distance culling for optimization
        if (m_distanceCullingEnabled) {
            cullDistantComponents();
        }

        m_lastUpdateTime = 0.0f;
    }

    void SoundSystem::registerEntity(std::shared_ptr<Entity> entity) {
        if (!entity) {
            Logger::getInstance().warning("Cannot register null entity to SoundSystem");
            return;
        }

        // Check if entity has a SoundComponent
        auto soundComponent = entity->getComponent<SoundComponent>();
        if (!soundComponent) {
            Logger::getInstance().warning("Entity has no SoundComponent to register");
            return;
        }

        // Add to entities list
        m_entities.push_back(entity);
        m_activeComponents.push_back(soundComponent.get());
        
        Logger::getInstance().info("Entity registered to SoundSystem");
    }

    void SoundSystem::unregisterEntity(std::shared_ptr<Entity> entity) {
        if (!entity) return;

        // Remove from entities list
        m_entities.erase(
            std::remove_if(m_entities.begin(), m_entities.end(),
                [entity](const std::weak_ptr<Entity>& weakEntity) {
                    auto locked = weakEntity.lock();
                    return !locked || locked == entity;
                }),
            m_entities.end()
        );

        // Remove from active components
        auto soundComponent = entity->getComponent<SoundComponent>();
        if (soundComponent) {
            m_activeComponents.erase(
                std::remove(m_activeComponents.begin(), m_activeComponents.end(), 
                           soundComponent.get()),
                m_activeComponents.end()
            );
        }

        Logger::getInstance().info("Entity unregistered from SoundSystem");
    }

    void SoundSystem::clear() {
        m_entities.clear();
        m_activeComponents.clear();
        Logger::getInstance().info("SoundSystem cleared");
    }

    void SoundSystem::setListenerPosition(const glm::vec3& position) {
        m_listenerPosition = position;
    }

    void SoundSystem::setListenerVelocity(const glm::vec3& velocity) {
        m_listenerVelocity = velocity;
    }

    void SoundSystem::setListenerOrientation(const glm::vec3& forward, const glm::vec3& up) {
        m_listenerForward = forward;
        m_listenerUp = up;
    }

    void SoundSystem::setGlobalVolume(float volume) {
        m_globalVolume = std::max(0.0f, std::min(1.0f, volume));
        alListenerf(AL_GAIN, m_globalVolume);
    }

    void SoundSystem::setDopplerFactor(float factor) {
        m_dopplerFactor = std::max(0.0f, factor);
        alDopplerFactor(m_dopplerFactor);
    }

    void SoundSystem::setSpeedOfSound(float speed) {
        m_speedOfSound = std::max(0.1f, speed);
        alSpeedOfSound(m_speedOfSound);
    }

    void SoundSystem::setDistanceCulling(bool enabled, float maxDistance) {
        m_distanceCullingEnabled = enabled;
        m_maxCullingDistance = maxDistance;
    }

    size_t SoundSystem::getActiveComponentCount() const {
        return m_activeComponents.size();
    }

    size_t SoundSystem::getPlayingComponentCount() const {
        size_t count = 0;
        for (const auto& component : m_activeComponents) {
            if (component && component->isPlaying()) {
                count++;
            }
        }
        return count;
    }

    void SoundSystem::updateComponents(float deltaTime) {
        // Update each sound component
        for (auto& component : m_activeComponents) {
            if (component) {
                component->update(deltaTime);
                component->updatePosition();
            }
        }
    }

    void SoundSystem::updateListener() {
        // Set listener position and velocity
        alListener3f(AL_POSITION, m_listenerPosition.x, m_listenerPosition.y, m_listenerPosition.z);
        alListener3f(AL_VELOCITY, m_listenerVelocity.x, m_listenerVelocity.y, m_listenerVelocity.z);
        
        // Set listener orientation
        float orientation[6] = {
            m_listenerForward.x, m_listenerForward.y, m_listenerForward.z,
            m_listenerUp.x, m_listenerUp.y, m_listenerUp.z
        };
        alListenerfv(AL_ORIENTATION, orientation);
        
        // Set global volume
        alListenerf(AL_GAIN, m_globalVolume);
    }

    void SoundSystem::cullDistantComponents() {
        // Remove components that are too far from listener
        m_activeComponents.erase(
            std::remove_if(m_activeComponents.begin(), m_activeComponents.end(),
                [this](SoundComponent* component) {
                    if (!component) return true;
                    
                    // Get component's entity
                    auto entity = component->getEntity().lock();
                    if (!entity) return true;
                    
                    // Get transform component
                    auto transform = entity->getComponent<TransformComponent>();
                    if (!transform) return false;
                    
                    // Calculate distance
                    float distance = calculateDistance(transform->getPosition());
                    
                    // Cull if too far
                    return distance > m_maxCullingDistance;
                }),
            m_activeComponents.end()
        );
    }

    void SoundSystem::cleanupInvalidEntities() {
        // Remove expired weak pointers
        m_entities.erase(
            std::remove_if(m_entities.begin(), m_entities.end(),
                [](const std::weak_ptr<Entity>& weakEntity) {
                    return weakEntity.expired();
                }),
            m_entities.end()
        );

        // Remove null components
        m_activeComponents.erase(
            std::remove(m_activeComponents.begin(), m_activeComponents.end(), nullptr),
            m_activeComponents.end()
        );
    }

    float SoundSystem::calculateDistance(const glm::vec3& soundPos) const {
        glm::vec3 diff = soundPos - m_listenerPosition;
        return glm::length(diff);
    }

} // namespace IKore
