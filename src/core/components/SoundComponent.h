#pragma once

#include "core/Component.h"
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <AL/al.h>
#include <AL/alc.h>

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
        ~SoundComponent() override;

        // Sound loading and management
        bool loadSound(const std::string& filename);
        void unloadSound();
        bool isLoaded() const { return m_isLoaded; }
        const std::string& getFilename() const { return m_filename; }
        
        // Playback control
        void play();
        void pause();
        void stop();
        void playOneShot(); // Play without interrupting current playback
        
        // Audio properties
        void setVolume(float volume);
        void setPitch(float pitch);
        void setLooping(bool looping);
        void setGain(float gain);
        
        // 3D Audio properties
        void setMaxDistance(float maxDistance);
        void setRolloffFactor(float rolloffFactor);
        void setReferenceDistance(float refDistance);
        void setMinGain(float minGain);
        void setMaxGain(float maxGain);
        
        // State queries
        bool isPlaying() const;
        bool isPaused() const;
        bool isStopped() const;
        
        // Property getters
        float getVolume() const { return m_volume; }
        float getPitch() const { return m_pitch; }
        bool isLooping() const { return m_looping; }
        float getGain() const { return m_gain; }
        float getMaxDistance() const { return m_maxDistance; }
        float getRolloffFactor() const { return m_rolloffFactor; }
        float getReferenceDistance() const { return m_referenceDistance; }
        
        // Component lifecycle
        void update(float deltaTime);
        
        // Position synchronization (called by system)
        void updatePosition();
        
        // Status queries
        bool isInitialized() const { return m_initialized; }
        bool isInFallbackMode() const { return m_fallbackMode; }
        
    protected:
        void onAttach() override;
        void onDetach() override;

    private:
        // OpenAL resources
        ALuint m_source;
        ALuint m_buffer;
        bool m_initialized;
        bool m_fallbackMode;  // True when running without audio hardware
        
        // Sound properties
        std::string m_filename;
        float m_volume;
        float m_pitch;
        float m_gain;
        bool m_looping;
        bool m_isLoaded;
        
        // 3D Audio properties
        float m_maxDistance;
        float m_rolloffFactor;
        float m_referenceDistance;
        float m_minGain;
        float m_maxGain;
        
        // Current position (synced from transform)
        glm::vec3 m_position;
        glm::vec3 m_velocity;
        
        // Internal state
        bool m_needsUpdate;
        float m_lastUpdateTime;
        
        // Helper methods
        bool initializeOpenAL();
        void cleanupOpenAL();
        void updateOpenALProperties();
        void syncPositionFromTransform();
        ALenum getOpenALState() const;
        
        // Static OpenAL context management
        static bool s_openALInitialized;
        static bool s_fallbackModeDetected;  // Global fallback detection
        static ALCdevice* s_device;
        static ALCcontext* s_context;
        static int s_componentCount;
        
        static bool initializeOpenALContext();
        static void cleanupOpenALContext();
    };

} // namespace IKore
