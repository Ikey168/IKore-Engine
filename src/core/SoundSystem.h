#pragma once

#include <memory>
#include <vector>
#include <set>
#include <glm/glm.hpp>

namespace IKore {

    class Entity;
    class SoundComponent;

    /**
     * @brief Sound System for managing SoundComponents in the ECS
     * 
     * This system manages all SoundComponents, providing centralized
     * updates and 3D audio listener management.
     * 
     * Features:
     * - Automatic SoundComponent updates
     * - 3D audio listener positioning
     * - Performance optimization through selective updates
     * - Global audio settings
     * - Distance culling for optimization
     * 
     * Usage:
     * @code
     * SoundSystem soundSystem;
     * soundSystem.initialize();
     * 
     * // Register entities with sound components
     * soundSystem.registerEntity(entity);
     * 
     * // Update each frame
     * soundSystem.update(deltaTime);
     * 
     * // Set listener position (usually camera position)
     * soundSystem.setListenerPosition(cameraPos);
     * @endcode
     */
    class SoundSystem {
    public:
        SoundSystem();
        ~SoundSystem();

        // System lifecycle
        bool initialize();
        void shutdown();
        void update(float deltaTime);

        // Entity management
        void registerEntity(std::shared_ptr<Entity> entity);
        void unregisterEntity(std::shared_ptr<Entity> entity);
        void clear();

        // Listener management (usually camera position)
        void setListenerPosition(const glm::vec3& position);
        void setListenerVelocity(const glm::vec3& velocity);
        void setListenerOrientation(const glm::vec3& forward, const glm::vec3& up);
        glm::vec3 getListenerPosition() const { return m_listenerPosition; }

        // Global audio settings
        void setGlobalVolume(float volume);
        void setDopplerFactor(float factor);
        void setSpeedOfSound(float speed);
        void setDistanceCulling(bool enabled, float maxDistance = 1000.0f);

        // State queries
        size_t getActiveComponentCount() const;
        size_t getPlayingComponentCount() const;
        bool isInitialized() const { return m_initialized; }

        // Performance settings
        void setUpdateFrequency(float frequency) { m_updateFrequency = frequency; }
        void setMaxActiveComponents(size_t maxComponents) { m_maxActiveComponents = maxComponents; }

    private:
        // System state
        bool m_initialized;
        float m_lastUpdateTime;
        float m_updateFrequency; // Hz, for optimization
        
        // Registered entities and components
        std::vector<std::weak_ptr<Entity>> m_entities;
        std::vector<SoundComponent*> m_activeComponents;
        
        // Listener state
        glm::vec3 m_listenerPosition;
        glm::vec3 m_listenerVelocity;
        glm::vec3 m_listenerForward;
        glm::vec3 m_listenerUp;
        
        // Global settings
        float m_globalVolume;
        float m_dopplerFactor;
        float m_speedOfSound;
        
        // Performance settings
        bool m_distanceCullingEnabled;
        float m_maxCullingDistance;
        size_t m_maxActiveComponents;
        
        // Internal methods
        void updateComponents(float deltaTime);
        void updateListener();
        void cullDistantComponents();
        void cleanupInvalidEntities();
        float calculateDistance(const glm::vec3& soundPos) const;
    };

} // namespace IKore
