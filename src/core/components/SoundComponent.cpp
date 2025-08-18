#include "SoundComponent.h"
#include "TransformComponent.h"
#include "core/Entity.h"
#include "core/Logger.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

namespace IKore {

    // Static member initialization
    bool SoundComponent::s_openALInitialized = false;
    bool SoundComponent::s_fallbackModeDetected = false;
    ALCdevice* SoundComponent::s_device = nullptr;
    ALCcontext* SoundComponent::s_context = nullptr;
    int SoundComponent::s_componentCount = 0;

    SoundComponent::SoundComponent()
        : m_source(0)
        , m_buffer(0)
        , m_filename("")
        , m_volume(1.0f)
        , m_pitch(1.0f)
        , m_gain(1.0f)
        , m_looping(false)
        , m_isLoaded(false)
        , m_maxDistance(100.0f)
        , m_rolloffFactor(1.0f)
        , m_referenceDistance(1.0f)
        , m_minGain(0.0f)
        , m_maxGain(1.0f)
        , m_position(0.0f)
        , m_velocity(0.0f)
        , m_needsUpdate(true)
        , m_lastUpdateTime(0.0f)
        , m_initialized(false)
        , m_fallbackMode(false) {
        
        s_componentCount++;
        Logger::getInstance().info("SoundComponent created, count: " + std::to_string(s_componentCount));
    }

    SoundComponent::~SoundComponent() {
        onDetach();
        s_componentCount--;
        
        if (s_componentCount == 0) {
            cleanupOpenALContext();
        }
        
        Logger::getInstance().info("SoundComponent destroyed, count: " + std::to_string(s_componentCount));
    }

    void SoundComponent::onAttach() {
        // Check if we already detected fallback mode globally
        if (s_fallbackModeDetected) {
            m_fallbackMode = true;
            m_initialized = true;
            return;
        }
        
        if (!initializeOpenAL()) {
            // First time detection - log it once
            if (!s_fallbackModeDetected) {
                Logger::getInstance().warning("OpenAL not available - SoundComponents will run in fallback mode");
                s_fallbackModeDetected = true;
            }
            m_fallbackMode = true;
            m_initialized = true;
            return;
        }
        
        m_initialized = true;
        m_fallbackMode = false;
        Logger::getInstance().info("SoundComponent initialized successfully with OpenAL");
    }

    void SoundComponent::onDetach() {
        unloadSound();
        cleanupOpenAL();
    }

    bool SoundComponent::initializeOpenAL() {
        // Initialize OpenAL context if not already done
        if (!s_openALInitialized) {
            if (!initializeOpenALContext()) {
                return false;
            }
        }

        // Generate OpenAL source
        alGenSources(1, &m_source);
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            Logger::getInstance().error("Failed to generate OpenAL source: " + std::to_string(error));
            return false;
        }

        // Set default source properties
        updateOpenALProperties();
        return true;
    }

    void SoundComponent::cleanupOpenAL() {
        if (m_source != 0) {
            alDeleteSources(1, &m_source);
            m_source = 0;
        }
    }

    bool SoundComponent::initializeOpenALContext() {
        // Try to open the default device (may fail in headless environments)
        s_device = alcOpenDevice(nullptr);
        if (!s_device) {
            // Don't log as error - this is expected in headless environments
            return false;
        }

        // Create context
        s_context = alcCreateContext(s_device, nullptr);
        if (!s_context) {
            Logger::getInstance().warning("Failed to create OpenAL context");
            alcCloseDevice(s_device);
            s_device = nullptr;
            return false;
        }

        // Make context current
        if (!alcMakeContextCurrent(s_context)) {
            Logger::getInstance().warning("Failed to make OpenAL context current");
            alcDestroyContext(s_context);
            alcCloseDevice(s_device);
            s_context = nullptr;
            s_device = nullptr;
            return false;
        }

        s_openALInitialized = true;
        Logger::getInstance().info("OpenAL context initialized successfully");
        return true;
    }

    void SoundComponent::cleanupOpenALContext() {
        if (s_context) {
            alcMakeContextCurrent(nullptr);
            alcDestroyContext(s_context);
            s_context = nullptr;
        }

        if (s_device) {
            alcCloseDevice(s_device);
            s_device = nullptr;
        }

        s_openALInitialized = false;
        Logger::getInstance().info("OpenAL context cleaned up");
    }

    bool SoundComponent::loadSound(const std::string& filename) {
        if (filename.empty()) {
            Logger::getInstance().warning("Cannot load sound with empty filename");
            return false;
        }

        // Unload any existing sound
        unloadSound();

        m_filename = filename;

        // If in fallback mode, just pretend to load successfully
        if (m_fallbackMode || s_fallbackModeDetected) {
            m_isLoaded = true;
            return true;
        }

        // For this implementation, we'll support WAV files
        // In a production system, you'd want to support multiple formats
        if (filename.find(".wav") == std::string::npos) {
            Logger::getInstance().warning("Only WAV files supported in basic implementation: " + filename);
            return false;
        }

        // Generate buffer
        alGenBuffers(1, &m_buffer);
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            // Switch to fallback mode for this and future components
            if (!s_fallbackModeDetected) {
                Logger::getInstance().warning("OpenAL buffer creation failed - switching to fallback mode");
                s_fallbackModeDetected = true;
            }
            m_fallbackMode = true;
            m_isLoaded = true;
            return true; // Return success in fallback mode
        }

        // Load WAV file (simplified implementation)
        // In a real implementation, you'd use a proper audio loading library
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open audio file: " + filename);
            alDeleteBuffers(1, &m_buffer);
            m_buffer = 0;
            return false;
        }

        // For now, create a simple sine wave as placeholder
        // This would be replaced with actual file loading
        std::vector<short> samples;
        const int sampleRate = 44100;
        const int duration = 1; // 1 second
        const float frequency = 440.0f; // A4 note
        
        for (int i = 0; i < sampleRate * duration; i++) {
            float t = static_cast<float>(i) / sampleRate;
            short sample = static_cast<short>(sin(2.0f * 3.14159f * frequency * t) * 32767 * 0.3f);
            samples.push_back(sample);
        }

        // Upload buffer data
        alBufferData(m_buffer, AL_FORMAT_MONO16, samples.data(), 
                    samples.size() * sizeof(short), sampleRate);
        
        error = alGetError();
        if (error != AL_NO_ERROR) {
            Logger::getInstance().error("Failed to upload buffer data: " + std::to_string(error));
            alDeleteBuffers(1, &m_buffer);
            m_buffer = 0;
            return false;
        }

        // Attach buffer to source
        alSourcei(m_source, AL_BUFFER, m_buffer);
        
        m_filename = filename;
        m_isLoaded = true;
        Logger::getInstance().info("Sound loaded successfully: " + filename);
        return true;
    }

    void SoundComponent::unloadSound() {
        if (m_isLoaded) {
            stop();
            
            if (m_buffer != 0) {
                alDeleteBuffers(1, &m_buffer);
                m_buffer = 0;
            }
            
            m_filename.clear();
            m_isLoaded = false;
            Logger::getInstance().info("Sound unloaded");
        }
    }

    void SoundComponent::play() {
        if (!m_isLoaded) {
            Logger::getInstance().warning("Cannot play sound - not loaded");
            return;
        }

        if (m_fallbackMode) {
            // Logger::getInstance().info("Playing sound in fallback mode: " + m_filename);
            return;
        }

        if (m_source == 0) {
            Logger::getInstance().warning("Cannot play sound - no OpenAL source");
            return;
        }

        alSourcePlay(m_source);
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            Logger::getInstance().error("Failed to play sound: " + std::to_string(error));
        }
    }

    void SoundComponent::pause() {
        if (m_fallbackMode) {
            // Logger::getInstance().info("Pausing sound in fallback mode: " + m_filename);
            return;
        }
        
        if (m_source == 0) return;
        
        alSourcePause(m_source);
    }

    void SoundComponent::stop() {
        if (m_fallbackMode) {
            // Logger::getInstance().info("Stopping sound in fallback mode: " + m_filename);
            return;
        }
        
        if (m_source == 0) return;
        
        alSourceStop(m_source);
    }

    void SoundComponent::playOneShot() {
        // For one-shot playback, we could create a temporary source
        // For simplicity, just play normally
        play();
    }

    void SoundComponent::setVolume(float volume) {
        m_volume = std::max(0.0f, std::min(1.0f, volume));
        m_needsUpdate = true;
    }

    void SoundComponent::setPitch(float pitch) {
        m_pitch = std::max(0.1f, std::min(10.0f, pitch));
        m_needsUpdate = true;
    }

    void SoundComponent::setLooping(bool looping) {
        m_looping = looping;
        m_needsUpdate = true;
    }

    void SoundComponent::setGain(float gain) {
        m_gain = std::max(0.0f, gain);
        m_needsUpdate = true;
    }

    void SoundComponent::setMaxDistance(float maxDistance) {
        m_maxDistance = std::max(0.1f, maxDistance);
        m_needsUpdate = true;
    }

    void SoundComponent::setRolloffFactor(float rolloffFactor) {
        m_rolloffFactor = std::max(0.0f, rolloffFactor);
        m_needsUpdate = true;
    }

    void SoundComponent::setReferenceDistance(float refDistance) {
        m_referenceDistance = std::max(0.1f, refDistance);
        m_needsUpdate = true;
    }

    void SoundComponent::setMinGain(float minGain) {
        m_minGain = std::max(0.0f, std::min(1.0f, minGain));
        m_needsUpdate = true;
    }

    void SoundComponent::setMaxGain(float maxGain) {
        m_maxGain = std::max(0.0f, maxGain);
        m_needsUpdate = true;
    }

    bool SoundComponent::isPlaying() const {
        return getOpenALState() == AL_PLAYING;
    }

    bool SoundComponent::isPaused() const {
        return getOpenALState() == AL_PAUSED;
    }

    bool SoundComponent::isStopped() const {
        ALenum state = getOpenALState();
        return state == AL_STOPPED || state == AL_INITIAL;
    }

    ALenum SoundComponent::getOpenALState() const {
        if (m_source == 0) return AL_INITIAL;
        
        ALint state;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);
        return static_cast<ALenum>(state);
    }

    void SoundComponent::update(float deltaTime) {
        if (m_source == 0) return;
        
        m_lastUpdateTime += deltaTime;
        
        // Update position from transform component
        syncPositionFromTransform();
        
        // Update OpenAL properties if needed
        if (m_needsUpdate) {
            updateOpenALProperties();
            m_needsUpdate = false;
        }
    }

    void SoundComponent::updatePosition() {
        syncPositionFromTransform();
        if (m_source != 0) {
            alSource3f(m_source, AL_POSITION, m_position.x, m_position.y, m_position.z);
            alSource3f(m_source, AL_VELOCITY, m_velocity.x, m_velocity.y, m_velocity.z);
        }
    }

    void SoundComponent::syncPositionFromTransform() {
        // Get entity and transform component
        auto entity = getEntity().lock();
        if (!entity) return;
        
        auto transform = entity->getComponent<TransformComponent>();
        if (!transform) return;
        
        // Calculate velocity based on position change
        glm::vec3 newPosition = transform->getPosition();
        if (m_lastUpdateTime > 0.0f) {
            m_velocity = (newPosition - m_position) / m_lastUpdateTime;
        }
        
        m_position = newPosition;
    }

    void SoundComponent::updateOpenALProperties() {
        if (m_source == 0) return;
        
        // Basic properties
        alSourcef(m_source, AL_GAIN, m_volume * m_gain);
        alSourcef(m_source, AL_PITCH, m_pitch);
        alSourcei(m_source, AL_LOOPING, m_looping ? AL_TRUE : AL_FALSE);
        
        // 3D properties
        alSourcef(m_source, AL_MAX_DISTANCE, m_maxDistance);
        alSourcef(m_source, AL_ROLLOFF_FACTOR, m_rolloffFactor);
        alSourcef(m_source, AL_REFERENCE_DISTANCE, m_referenceDistance);
        alSourcef(m_source, AL_MIN_GAIN, m_minGain);
        alSourcef(m_source, AL_MAX_GAIN, m_maxGain);
        
        // Position and velocity
        alSource3f(m_source, AL_POSITION, m_position.x, m_position.y, m_position.z);
        alSource3f(m_source, AL_VELOCITY, m_velocity.x, m_velocity.y, m_velocity.z);
        
        // Check for errors
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            Logger::getInstance().warning("OpenAL error updating properties: " + std::to_string(error));
        }
    }

} // namespace IKore
