#pragma once

#include <AL/al.h>
#include <AL/alc.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <thread>
#include <mutex>
#include <functional>

namespace IKore {

    /**
     * @brief 3D Audio source configuration and state
     */
    struct AudioSource {
        ALuint sourceId;
        ALuint bufferId;
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 direction;
        
        // Audio properties
        float gain = 1.0f;
        float pitch = 1.0f;
        float rolloffFactor = 1.0f;
        float maxDistance = 100.0f;
        float referenceDistance = 1.0f;
        bool looping = false;
        bool streaming = false;
        bool is3D = true;
        
        // State tracking
        bool isPlaying = false;
        bool isPaused = false;
        std::string audioFile;
        
        AudioSource() : sourceId(0), bufferId(0), position(0.0f), velocity(0.0f), direction(0.0f, 0.0f, -1.0f) {}
    };

    /**
     * @brief Audio listener (usually the camera/player)
     */
    struct AudioListener {
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 orientation[2]; // at and up vectors
        float gain = 1.0f;
        
        AudioListener() : position(0.0f), velocity(0.0f), gain(1.0f) {
            orientation[0] = glm::vec3(0.0f, 0.0f, -1.0f); // at
            orientation[1] = glm::vec3(0.0f, 1.0f, 0.0f);  // up
        }
    };

    /**
     * @brief Audio buffer management for loaded sounds
     */
    struct AudioBuffer {
        ALuint bufferId;
        std::string filename;
        ALenum format;
        ALsizei size;
        ALsizei frequency;
        bool isLoaded = false;
        
        AudioBuffer() : bufferId(0), format(AL_FORMAT_MONO16), size(0), frequency(44100) {}
    };

    /**
     * @brief OpenAL 3D Positional Audio System
     * 
     * This system provides comprehensive 3D audio capabilities:
     * - 3D positional audio with distance attenuation
     * - Directional audio sources and listener orientation
     * - Real-time audio streaming and static buffers
     * - Multiple audio format support (WAV, OGG, MP3)
     * - Performance optimized for real-time applications
     * 
     * Features:
     * - Distance-based volume attenuation
     * - Doppler effect simulation
     * - Environmental audio effects
     * - Multi-threaded audio processing
     * - Memory efficient buffer management
     * 
     * Usage Examples:
     * 
     * @code
     * // Initialize audio system
     * OpenALAudioEngine audio;
     * audio.initialize();
     * 
     * // Load and play 3D positioned sound
     * auto sourceId = audio.loadSound("explosion.wav");
     * audio.setSourcePosition(sourceId, {10.0f, 0.0f, 5.0f});
     * audio.playSound(sourceId);
     * 
     * // Update listener position (usually camera)
     * audio.setListenerPosition({0.0f, 0.0f, 0.0f});
     * audio.setListenerOrientation({0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f});
     * 
     * // Streaming audio for background music
     * auto musicId = audio.loadStreamingSound("background_music.ogg");
     * audio.playStreamingSound(musicId, true); // looping
     * @endcode
     */
    class OpenALAudioEngine {
    public:
        OpenALAudioEngine();
        ~OpenALAudioEngine();

        // Core system management
        bool initialize();
        void shutdown();
        bool isInitialized() const { return m_initialized; }
        
        // Audio loading and management
        uint32_t loadSound(const std::string& filename);
        uint32_t loadStreamingSound(const std::string& filename);
        void unloadSound(uint32_t sourceId);
        void unloadAllSounds();
        
        // 3D Audio playback control
        void playSound(uint32_t sourceId);
        void pauseSound(uint32_t sourceId);
        void stopSound(uint32_t sourceId);
        void stopAllSounds();
        
        // Streaming audio control
        void playStreamingSound(uint32_t sourceId, bool loop = false);
        void pauseStreamingSound(uint32_t sourceId);
        void stopStreamingSound(uint32_t sourceId);
        
        // 3D Positional audio properties
        void setSourcePosition(uint32_t sourceId, const glm::vec3& position);
        void setSourceVelocity(uint32_t sourceId, const glm::vec3& velocity);
        void setSourceDirection(uint32_t sourceId, const glm::vec3& direction);
        glm::vec3 getSourcePosition(uint32_t sourceId) const;
        
        // Audio source properties
        void setSourceGain(uint32_t sourceId, float gain);
        void setSourcePitch(uint32_t sourceId, float pitch);
        void setSourceLooping(uint32_t sourceId, bool looping);
        void setSourceRolloffFactor(uint32_t sourceId, float rolloff);
        void setSourceMaxDistance(uint32_t sourceId, float maxDistance);
        void setSourceReferenceDistance(uint32_t sourceId, float refDistance);
        
        // Listener (camera/player) properties
        void setListenerPosition(const glm::vec3& position);
        void setListenerVelocity(const glm::vec3& velocity);
        void setListenerOrientation(const glm::vec3& at, const glm::vec3& up);
        void setListenerGain(float gain);
        glm::vec3 getListenerPosition() const;
        
        // Audio state queries
        bool isPlaying(uint32_t sourceId) const;
        bool isPaused(uint32_t sourceId) const;
        float getSourceGain(uint32_t sourceId) const;
        float getSourcePitch(uint32_t sourceId) const;
        
        // Performance and debugging
        void update(); // Call this each frame for streaming audio
        size_t getActiveSourceCount() const;
        size_t getLoadedBufferCount() const;
        std::vector<std::string> getLoadedSounds() const;
        
        // Environmental audio effects
        void setGlobalVolume(float volume);
        float getGlobalVolume() const;
        void setDopplerFactor(float factor);
        void setSpeedOfSound(float speed);
        
        // Error handling and diagnostics
        std::string getLastError() const;
        bool checkALError(const std::string& operation = "");
        void printAudioDeviceInfo() const;

    private:
        // OpenAL context and device
        ALCdevice* m_device;
        ALCcontext* m_context;
        bool m_initialized;
        
        // Audio data management
        std::unordered_map<uint32_t, std::unique_ptr<AudioSource>> m_audioSources;
        std::unordered_map<std::string, std::unique_ptr<AudioBuffer>> m_audioBuffers;
        std::unique_ptr<AudioListener> m_listener;
        
        // ID management
        uint32_t m_nextSourceId;
        std::mutex m_audioMutex;
        
        // Performance tracking
        size_t m_maxConcurrentSources;
        float m_globalVolume;
        std::string m_lastError;
        
        // Streaming audio support
        std::thread m_streamingThread;
        bool m_streamingActive;
        std::vector<uint32_t> m_streamingSources;
        
        // Internal helper methods
        bool createAudioSource(uint32_t sourceId);
        bool loadAudioFile(const std::string& filename, AudioBuffer& buffer);
        void cleanupSource(uint32_t sourceId);
        void updateStreamingSources();
        void streamingThreadFunction();
        
        // Audio format support
        bool loadWAV(const std::string& filename, AudioBuffer& buffer);
        bool loadOGG(const std::string& filename, AudioBuffer& buffer);
        bool loadMP3(const std::string& filename, AudioBuffer& buffer);
        
        // Distance and attenuation calculations
        float calculateDistanceAttenuation(const glm::vec3& sourcePos, const glm::vec3& listenerPos, 
                                         float referenceDistance, float maxDistance, float rolloffFactor);
        glm::vec3 calculateDopplerEffect(const glm::vec3& sourceVel, const glm::vec3& listenerVel,
                                       const glm::vec3& sourcePos, const glm::vec3& listenerPos);
        
        // OpenAL error checking
        void logALError(ALenum error, const std::string& operation) const;
        void logALCError(ALCdevice* device, const std::string& operation) const;
    };

    /**
     * @brief Audio System singleton for global access
     */
    class AudioSystem {
    public:
        static AudioSystem& getInstance();
        
        OpenALAudioEngine& getEngine() { return m_audioEngine; }
        const OpenALAudioEngine& getEngine() const { return m_audioEngine; }
        
        // Convenience methods that delegate to the engine
        bool initialize() { return m_audioEngine.initialize(); }
        void shutdown() { m_audioEngine.shutdown(); }
        void update() { m_audioEngine.update(); }
        
        uint32_t loadSound(const std::string& filename) { return m_audioEngine.loadSound(filename); }
        void playSound(uint32_t sourceId) { m_audioEngine.playSound(sourceId); }
        void setSourcePosition(uint32_t sourceId, const glm::vec3& position) { 
            m_audioEngine.setSourcePosition(sourceId, position); 
        }
        void setListenerPosition(const glm::vec3& position) { 
            m_audioEngine.setListenerPosition(position); 
        }
        
    private:
        AudioSystem() = default;
        ~AudioSystem() = default;
        AudioSystem(const AudioSystem&) = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;
        
        OpenALAudioEngine m_audioEngine;
    };

} // namespace IKore
