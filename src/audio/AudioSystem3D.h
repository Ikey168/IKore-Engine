#pragma once

#include "OpenALAudioEngine.h"
#include "core/Entity.h"
#include <memory>
#include <vector>
#include <set>
#include <string>

namespace IKore {

    class AudioComponent;
    class AudioListenerComponent;
    
    // Forward declaration for ambient sound zones
    struct AmbientSoundZone;
    class AmbientSoundZoneManager;

    /**
     * @brief Audio System for managing 3D positional audio in the ECS
     * 
     * This system coordinates between the OpenAL audio engine and audio components,
     * providing automatic updates and management of 3D audio sources and listeners.
     * 
     * Features:
     * - Automatic AudioComponent and AudioListenerComponent management
     * - Performance optimization through selective updates
     * - Integration with the Entity-Component System
     * - Centralized audio configuration and debugging
     * - Support for multiple listeners (though only one active at a time)
     * 
     * Usage:
     * 
     * @code
     * // Initialize audio system
     * AudioSystem3D audioSystem;
     * audioSystem.initialize();
     * 
     * // Register entities with audio components
     * audioSystem.registerAudioEntity(entity);
     * 
     * // Update each frame
     * audioSystem.update(deltaTime);
     * 
     * // Set global audio properties
     * audioSystem.setGlobalVolume(0.8f);
     * audioSystem.setDopplerFactor(1.0f);
     * @endcode
     */
    class AudioSystem3D {
    public:
        AudioSystem3D();
        ~AudioSystem3D();

        // System lifecycle
        bool initialize();
        void shutdown();
        bool isInitialized() const { return m_initialized; }
        
        // Entity management
        void registerAudioEntity(std::shared_ptr<Entity> entity);
        void unregisterAudioEntity(std::shared_ptr<Entity> entity);
        void registerListenerEntity(std::shared_ptr<Entity> entity);
        void unregisterListenerEntity(std::shared_ptr<Entity> entity);
        
        // System update
        void update(float deltaTime);
        
        // Global audio settings
        void setGlobalVolume(float volume);
        float getGlobalVolume() const;
        void setDopplerFactor(float factor);
        void setSpeedOfSound(float speed);
        
        // Listener management
        void setActiveListener(std::shared_ptr<Entity> listenerEntity);
        std::shared_ptr<Entity> getActiveListener() const { return m_activeListener.lock(); }
        
        // Convenience methods for immediate audio playback
        uint32_t playSound3D(const std::string& filename, const glm::vec3& position, 
                            float volume = 1.0f, float pitch = 1.0f);
        uint32_t playSound2D(const std::string& filename, float volume = 1.0f, float pitch = 1.0f);
        void stopSound(uint32_t sourceId);
        
        // Performance and debugging
        size_t getActiveAudioEntityCount() const;
        size_t getActiveSoundSourceCount() const;
        std::vector<std::string> getLoadedSounds() const;
        void printAudioStatistics() const;
        
        // Audio engine access
        OpenALAudioEngine& getAudioEngine() { return AudioSystem::getInstance().getEngine(); }
        const OpenALAudioEngine& getAudioEngine() const { return AudioSystem::getInstance().getEngine(); }

        // Ambient zone API
        void addAmbientZone(const AmbientSoundZone& zone);
        void clearAmbientZones();

    private:
        bool m_initialized;
        
        // Entity tracking
        std::set<std::weak_ptr<Entity>, std::owner_less<std::weak_ptr<Entity>>> m_audioEntities;
        std::set<std::weak_ptr<Entity>, std::owner_less<std::weak_ptr<Entity>>> m_listenerEntities;
        std::weak_ptr<Entity> m_activeListener;
        
        // Performance tracking
        float m_updateTimer;
        size_t m_frameCount;
        
        // Ambient sound zones
        AmbientSoundZoneManager* m_ambientZoneManager;
        
        // Internal helpers
        void updateAudioComponents();
        void updateListenerComponents();
        void cleanupDeadEntities();
        bool isEntityValid(const std::weak_ptr<Entity>& entity) const;
    };

    /**
     * @brief Audio Manager for high-level audio operations
     * 
     * Provides simplified interface for common audio operations without
     * requiring direct ECS integration.
     */
    class AudioManager {
    public:
        static AudioManager& getInstance();
        
        // Simple audio playback
        static void playSound(const std::string& filename, const glm::vec3& position = glm::vec3(0.0f));
        static void playMusic(const std::string& filename, bool loop = true);
        static void stopMusic();
        static void setMusicVolume(float volume);
        static void setSFXVolume(float volume);
        
        // Global audio control
        static void pauseAll();
        static void resumeAll();
        static void stopAll();
        
    private:
        AudioManager() = default;
        ~AudioManager() = default;
        AudioManager(const AudioManager&) = delete;
        AudioManager& operator=(const AudioManager&) = delete;
        
        uint32_t m_musicSourceId = 0;
        float m_musicVolume = 1.0f;
        float m_sfxVolume = 1.0f;
        bool m_paused = false;
    };

} // namespace IKore
