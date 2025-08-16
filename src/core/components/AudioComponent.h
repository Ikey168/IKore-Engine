#pragma once

#include "core/Component.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace IKore {

    /**
     * @brief 3D Audio Source Component for entities
     * 
     * This component enables entities to emit 3D positional audio.
     * The audio will automatically adjust based on the entity's position
     * relative to the listener (usually the camera/player).
     * 
     * Features:
     * - Automatic 3D positioning based on entity transform
     * - Distance-based volume attenuation
     * - Directional audio with configurable cone angles
     * - Multiple audio clips per entity
     * - Looping and one-shot playback modes
     * - Dynamic audio property adjustment
     * 
     * Usage Examples:
     * 
     * @code
     * // Add audio to an entity
     * auto entity = std::make_shared<Entity>();
     * auto audioComp = entity->addComponent<AudioComponent>();
     * 
     * // Load a sound effect
     * audioComp->loadSound("explosion.wav", "explosion");
     * audioComp->play("explosion");
     * 
     * // Configure 3D properties
     * audioComp->setMaxDistance(50.0f);
     * audioComp->setRolloffFactor(2.0f);
     * 
     * // Directional audio (like a speaker)
     * audioComp->setDirectional(true);
     * audioComp->setConeAngles(45.0f, 90.0f);
     * @endcode
     */
    class AudioComponent : public Component {
    public:
        AudioComponent();
        ~AudioComponent() override;

        // Audio clip management
        bool loadSound(const std::string& filename, const std::string& clipName);
        bool loadStreamingSound(const std::string& filename, const std::string& clipName);
        void unloadSound(const std::string& clipName);
        void unloadAllSounds();
        
        // Playback control
        void play(const std::string& clipName);
        void pause(const std::string& clipName);
        void stop(const std::string& clipName);
        void stopAll();
        
        // Playback with options
        void playOneShot(const std::string& clipName, float volume = 1.0f, float pitch = 1.0f);
        void playLooped(const std::string& clipName);
        void playAtPosition(const std::string& clipName, const glm::vec3& position);
        
        // Audio source properties
        void setVolume(float volume);
        void setPitch(float pitch);
        void setLooping(bool looping);
        void setMuted(bool muted);
        
        // 3D Audio properties
        void setMaxDistance(float maxDistance);
        void setRolloffFactor(float rolloffFactor);
        void setReferenceDistance(float refDistance);
        void setVelocity(const glm::vec3& velocity);
        
        // Directional audio
        void setDirectional(bool directional);
        void setDirection(const glm::vec3& direction);
        void setConeAngles(float innerAngle, float outerAngle);
        void setConeOuterGain(float gain);
        
        // State queries
        bool isPlaying(const std::string& clipName) const;
        bool isPaused(const std::string& clipName) const;
        bool hasSound(const std::string& clipName) const;
        std::vector<std::string> getLoadedSounds() const;
        
        // Property getters
        float getVolume() const { return m_volume; }
        float getPitch() const { return m_pitch; }
        bool isLooping() const { return m_looping; }
        bool isMuted() const { return m_muted; }
        bool isDirectional() const { return m_directional; }
        glm::vec3 getDirection() const { return m_direction; }
        float getMaxDistance() const { return m_maxDistance; }
        float getRolloffFactor() const { return m_rolloffFactor; }
        
        // Update method (called by AudioSystem)
        void update();
        
        // Position synchronization with entity transform
        void setPosition(const glm::vec3& position);
        glm::vec3 getPosition() const { return m_position; }

    private:
        // Audio source management
        struct AudioClip {
            uint32_t sourceId;
            std::string filename;
            std::string clipName;
            bool isStreaming;
            bool isLoaded;
            
            AudioClip() : sourceId(0), isStreaming(false), isLoaded(false) {}
        };
        
        std::unordered_map<std::string, std::unique_ptr<AudioClip>> m_audioClips;
        
        // 3D Audio properties
        glm::vec3 m_position;
        glm::vec3 m_velocity;
        glm::vec3 m_direction;
        
        // Audio properties
        float m_volume;
        float m_pitch;
        float m_maxDistance;
        float m_rolloffFactor;
        float m_referenceDistance;
        bool m_looping;
        bool m_muted;
        bool m_enabled;
        
        // Directional audio
        bool m_directional;
        float m_coneInnerAngle;
        float m_coneOuterAngle;
        float m_coneOuterGain;
        
        // Internal helpers
        AudioClip* getClip(const std::string& clipName);
        const AudioClip* getClip(const std::string& clipName) const;
        void updateSourceProperties(AudioClip* clip);
        void syncPositionWithEntity();
    };

    /**
     * @brief Audio Listener Component for camera/player entities
     * 
     * This component represents the audio listener in the 3D audio system.
     * Usually attached to the camera or player entity to define where
     * sounds are heard from.
     * 
     * Features:
     * - Automatic position and orientation sync with entity
     * - Global volume control
     * - Velocity tracking for Doppler effects
     * - Easy switching between different listeners
     * 
     * Usage:
     * 
     * @code
     * // Add listener to camera entity
     * auto camera = std::make_shared<Entity>();
     * auto listenerComp = camera->addComponent<AudioListenerComponent>();
     * listenerComp->setActive(true);
     * @endcode
     */
    class AudioListenerComponent : public Component {
    public:
        AudioListenerComponent();
        ~AudioListenerComponent() override;

        // Listener control
        void setActive(bool active);
        bool isActive() const { return m_active; }
        
        // Audio properties
        void setGlobalVolume(float volume);
        float getGlobalVolume() const { return m_globalVolume; }
        
        // 3D properties
        void setVelocity(const glm::vec3& velocity);
        glm::vec3 getVelocity() const { return m_velocity; }
        
        // Update method
        void update();
        
        // Position and orientation sync
        void setPosition(const glm::vec3& position);
        void setOrientation(const glm::vec3& at, const glm::vec3& up);
        glm::vec3 getPosition() const { return m_position; }

    private:
        bool m_active;
        glm::vec3 m_position;
        glm::vec3 m_velocity;
        glm::vec3 m_orientationAt;
        glm::vec3 m_orientationUp;
        float m_globalVolume;
        
        void syncWithAudioSystem();
        void syncPositionWithEntity();
    };

} // namespace IKore
