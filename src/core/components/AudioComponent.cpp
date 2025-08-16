#include "AudioComponent.h"
#include "audio/OpenALAudioEngine.h"
#include "core/Entity.h"
#include "core/components/TransformComponent.h"
#include <iostream>
#include <algorithm>

namespace IKore {

    // AudioComponent Implementation

    AudioComponent::AudioComponent() 
        : m_position(0.0f)
        , m_velocity(0.0f)
        , m_direction(0.0f, 0.0f, -1.0f)
        , m_volume(1.0f)
        , m_pitch(1.0f)
        , m_maxDistance(100.0f)
        , m_rolloffFactor(1.0f)
        , m_referenceDistance(1.0f)
        , m_looping(false)
        , m_muted(false)
        , m_enabled(true)
        , m_directional(false)
        , m_coneInnerAngle(360.0f)
        , m_coneOuterAngle(360.0f)
        , m_coneOuterGain(0.0f) {
    }

    AudioComponent::~AudioComponent() {
        unloadAllSounds();
    }

    bool AudioComponent::loadSound(const std::string& filename, const std::string& clipName) {
        if (!m_enabled) {
            return false;
        }

        // Check if clip already exists
        if (m_audioClips.find(clipName) != m_audioClips.end()) {
            std::cout << "Audio clip '" << clipName << "' already loaded, replacing..." << std::endl;
            unloadSound(clipName);
        }

        // Load sound through audio system
        auto& audioSystem = AudioSystem::getInstance();
        uint32_t sourceId = audioSystem.loadSound(filename);
        
        if (sourceId == 0) {
            std::cerr << "Failed to load audio file: " << filename << std::endl;
            return false;
        }

        // Create and configure audio clip
        auto clip = std::make_unique<AudioClip>();
        clip->sourceId = sourceId;
        clip->filename = filename;
        clip->clipName = clipName;
        clip->isStreaming = false;
        clip->isLoaded = true;

        // Apply current component properties to the source
        updateSourceProperties(clip.get());

        m_audioClips[clipName] = std::move(clip);
        
        std::cout << "Loaded 3D audio clip: " << clipName << " (" << filename << ")" << std::endl;
        return true;
    }

    bool AudioComponent::loadStreamingSound(const std::string& filename, const std::string& clipName) {
        if (!m_enabled) {
            return false;
        }

        // Check if clip already exists
        if (m_audioClips.find(clipName) != m_audioClips.end()) {
            std::cout << "Streaming audio clip '" << clipName << "' already loaded, replacing..." << std::endl;
            unloadSound(clipName);
        }

        // Load streaming sound through audio system
        auto& audioSystem = AudioSystem::getInstance();
        uint32_t sourceId = audioSystem.getEngine().loadStreamingSound(filename);
        
        if (sourceId == 0) {
            std::cerr << "Failed to load streaming audio file: " << filename << std::endl;
            return false;
        }

        // Create and configure streaming audio clip
        auto clip = std::make_unique<AudioClip>();
        clip->sourceId = sourceId;
        clip->filename = filename;
        clip->clipName = clipName;
        clip->isStreaming = true;
        clip->isLoaded = true;

        // Apply current component properties to the source
        updateSourceProperties(clip.get());

        m_audioClips[clipName] = std::move(clip);
        
        std::cout << "Loaded streaming 3D audio clip: " << clipName << " (" << filename << ")" << std::endl;
        return true;
    }

    void AudioComponent::unloadSound(const std::string& clipName) {
        auto it = m_audioClips.find(clipName);
        if (it != m_audioClips.end()) {
            // Stop the sound first
            stop(clipName);
            
            // Unload from audio system
            auto& audioSystem = AudioSystem::getInstance();
            audioSystem.getEngine().unloadSound(it->second->sourceId);
            
            // Remove from our collection
            m_audioClips.erase(it);
            
            std::cout << "Unloaded audio clip: " << clipName << std::endl;
        }
    }

    void AudioComponent::unloadAllSounds() {
        for (auto& pair : m_audioClips) {
            stop(pair.first);
            
            auto& audioSystem = AudioSystem::getInstance();
            audioSystem.getEngine().unloadSound(pair.second->sourceId);
        }
        
        m_audioClips.clear();
        std::cout << "Unloaded all audio clips from AudioComponent" << std::endl;
    }

    void AudioComponent::play(const std::string& clipName) {
        if (!m_enabled || m_muted) {
            return;
        }

        AudioClip* clip = getClip(clipName);
        if (!clip) {
            std::cerr << "Audio clip not found: " << clipName << std::endl;
            return;
        }

        auto& audioSystem = AudioSystem::getInstance();
        audioSystem.playSound(clip->sourceId);
    }

    void AudioComponent::pause(const std::string& clipName) {
        AudioClip* clip = getClip(clipName);
        if (!clip) {
            return;
        }

        auto& audioSystem = AudioSystem::getInstance();
        audioSystem.getEngine().pauseSound(clip->sourceId);
    }

    void AudioComponent::stop(const std::string& clipName) {
        AudioClip* clip = getClip(clipName);
        if (!clip) {
            return;
        }

        auto& audioSystem = AudioSystem::getInstance();
        audioSystem.getEngine().stopSound(clip->sourceId);
    }

    void AudioComponent::stopAll() {
        auto& audioSystem = AudioSystem::getInstance();
        
        for (auto& pair : m_audioClips) {
            audioSystem.getEngine().stopSound(pair.second->sourceId);
        }
    }

    void AudioComponent::playOneShot(const std::string& clipName, float volume, float pitch) {
        if (!m_enabled || m_muted) {
            return;
        }

        AudioClip* clip = getClip(clipName);
        if (!clip) {
            std::cerr << "Audio clip not found: " << clipName << std::endl;
            return;
        }

        auto& audioSystem = AudioSystem::getInstance();
        
        // Temporarily adjust properties
        float originalVolume = m_volume;
        float originalPitch = m_pitch;
        bool originalLooping = m_looping;
        
        // Set one-shot properties
        audioSystem.getEngine().setSourceGain(clip->sourceId, volume * m_volume);
        audioSystem.getEngine().setSourcePitch(clip->sourceId, pitch * m_pitch);
        audioSystem.getEngine().setSourceLooping(clip->sourceId, false);
        
        // Play the sound
        audioSystem.playSound(clip->sourceId);
        
        // Note: In a full implementation, we might want to restore original properties
        // after the sound finishes, but for simplicity we'll leave them as-is
    }

    void AudioComponent::playLooped(const std::string& clipName) {
        AudioClip* clip = getClip(clipName);
        if (!clip) {
            std::cerr << "Audio clip not found: " << clipName << std::endl;
            return;
        }

        auto& audioSystem = AudioSystem::getInstance();
        audioSystem.getEngine().setSourceLooping(clip->sourceId, true);
        play(clipName);
    }

    void AudioComponent::playAtPosition(const std::string& clipName, const glm::vec3& position) {
        AudioClip* clip = getClip(clipName);
        if (!clip) {
            std::cerr << "Audio clip not found: " << clipName << std::endl;
            return;
        }

        auto& audioSystem = AudioSystem::getInstance();
        
        // Temporarily set position
        glm::vec3 originalPosition = m_position;
        audioSystem.setSourcePosition(clip->sourceId, position);
        
        // Play the sound
        play(clipName);
        
        // Restore original position
        audioSystem.setSourcePosition(clip->sourceId, originalPosition);
    }

    void AudioComponent::setVolume(float volume) {
        m_volume = std::max(0.0f, std::min(1.0f, volume));
        
        // Update all loaded sources
        auto& audioSystem = AudioSystem::getInstance();
        for (auto& pair : m_audioClips) {
            audioSystem.getEngine().setSourceGain(pair.second->sourceId, m_muted ? 0.0f : m_volume);
        }
    }

    void AudioComponent::setPitch(float pitch) {
        m_pitch = std::max(0.01f, std::min(4.0f, pitch));
        
        // Update all loaded sources
        auto& audioSystem = AudioSystem::getInstance();
        for (auto& pair : m_audioClips) {
            audioSystem.getEngine().setSourcePitch(pair.second->sourceId, m_pitch);
        }
    }

    void AudioComponent::setLooping(bool looping) {
        m_looping = looping;
        
        // Update all loaded sources
        auto& audioSystem = AudioSystem::getInstance();
        for (auto& pair : m_audioClips) {
            audioSystem.getEngine().setSourceLooping(pair.second->sourceId, m_looping);
        }
    }

    void AudioComponent::setMuted(bool muted) {
        m_muted = muted;
        
        // Update volume on all sources
        auto& audioSystem = AudioSystem::getInstance();
        for (auto& pair : m_audioClips) {
            audioSystem.getEngine().setSourceGain(pair.second->sourceId, m_muted ? 0.0f : m_volume);
        }
    }

    void AudioComponent::setMaxDistance(float maxDistance) {
        m_maxDistance = std::max(0.1f, maxDistance);
        
        // Update all loaded sources
        auto& audioSystem = AudioSystem::getInstance();
        for (auto& pair : m_audioClips) {
            audioSystem.getEngine().setSourceMaxDistance(pair.second->sourceId, m_maxDistance);
        }
    }

    void AudioComponent::setRolloffFactor(float rolloffFactor) {
        m_rolloffFactor = std::max(0.0f, rolloffFactor);
        
        // Update all loaded sources
        auto& audioSystem = AudioSystem::getInstance();
        for (auto& pair : m_audioClips) {
            audioSystem.getEngine().setSourceRolloffFactor(pair.second->sourceId, m_rolloffFactor);
        }
    }

    void AudioComponent::setReferenceDistance(float refDistance) {
        m_referenceDistance = std::max(0.1f, refDistance);
        
        // Update all loaded sources
        auto& audioSystem = AudioSystem::getInstance();
        for (auto& pair : m_audioClips) {
            audioSystem.getEngine().setSourceReferenceDistance(pair.second->sourceId, m_referenceDistance);
        }
    }

    void AudioComponent::setVelocity(const glm::vec3& velocity) {
        m_velocity = velocity;
        
        // Update all loaded sources
        auto& audioSystem = AudioSystem::getInstance();
        for (auto& pair : m_audioClips) {
            audioSystem.getEngine().setSourceVelocity(pair.second->sourceId, m_velocity);
        }
    }

    void AudioComponent::setDirectional(bool directional) {
        m_directional = directional;
        // Directional audio configuration would be implemented here
        // OpenAL supports cone-based directional audio
    }

    void AudioComponent::setDirection(const glm::vec3& direction) {
        m_direction = glm::normalize(direction);
        
        // Update all loaded sources
        auto& audioSystem = AudioSystem::getInstance();
        for (auto& pair : m_audioClips) {
            audioSystem.getEngine().setSourceDirection(pair.second->sourceId, m_direction);
        }
    }

    void AudioComponent::setConeAngles(float innerAngle, float outerAngle) {
        m_coneInnerAngle = std::max(0.0f, std::min(360.0f, innerAngle));
        m_coneOuterAngle = std::max(m_coneInnerAngle, std::min(360.0f, outerAngle));
        
        // Note: OpenAL cone configuration would be applied here
        // This requires additional OpenAL source properties
    }

    void AudioComponent::setConeOuterGain(float gain) {
        m_coneOuterGain = std::max(0.0f, std::min(1.0f, gain));
        // Apply to all sources when directional audio is enabled
    }

    bool AudioComponent::isPlaying(const std::string& clipName) const {
        const AudioClip* clip = getClip(clipName);
        if (!clip) {
            return false;
        }

        auto& audioSystem = AudioSystem::getInstance();
        return audioSystem.getEngine().isPlaying(clip->sourceId);
    }

    bool AudioComponent::isPaused(const std::string& clipName) const {
        const AudioClip* clip = getClip(clipName);
        if (!clip) {
            return false;
        }

        auto& audioSystem = AudioSystem::getInstance();
        return audioSystem.getEngine().isPaused(clip->sourceId);
    }

    bool AudioComponent::hasSound(const std::string& clipName) const {
        return m_audioClips.find(clipName) != m_audioClips.end();
    }

    std::vector<std::string> AudioComponent::getLoadedSounds() const {
        std::vector<std::string> sounds;
        sounds.reserve(m_audioClips.size());
        
        for (const auto& pair : m_audioClips) {
            sounds.push_back(pair.first);
        }
        
        return sounds;
    }

    void AudioComponent::update() {
        if (!m_enabled) {
            return;
        }

        // Sync position with entity's transform
        syncPositionWithEntity();
        
        // Update position for all audio sources
        auto& audioSystem = AudioSystem::getInstance();
        for (auto& pair : m_audioClips) {
            audioSystem.setSourcePosition(pair.second->sourceId, m_position);
        }
    }

    void AudioComponent::setPosition(const glm::vec3& position) {
        m_position = position;
        
        // Update all loaded sources
        auto& audioSystem = AudioSystem::getInstance();
        for (auto& pair : m_audioClips) {
            audioSystem.setSourcePosition(pair.second->sourceId, m_position);
        }
    }

    // Private helper methods

    AudioComponent::AudioClip* AudioComponent::getClip(const std::string& clipName) {
        auto it = m_audioClips.find(clipName);
        return (it != m_audioClips.end()) ? it->second.get() : nullptr;
    }

    const AudioComponent::AudioClip* AudioComponent::getClip(const std::string& clipName) const {
        auto it = m_audioClips.find(clipName);
        return (it != m_audioClips.end()) ? it->second.get() : nullptr;
    }

    void AudioComponent::updateSourceProperties(AudioClip* clip) {
        if (!clip || !clip->isLoaded) {
            return;
        }

        auto& audioSystem = AudioSystem::getInstance();
        
        // Apply all current properties to the source
        audioSystem.getEngine().setSourceGain(clip->sourceId, m_muted ? 0.0f : m_volume);
        audioSystem.getEngine().setSourcePitch(clip->sourceId, m_pitch);
        audioSystem.getEngine().setSourceLooping(clip->sourceId, m_looping);
        audioSystem.setSourcePosition(clip->sourceId, m_position);
        audioSystem.getEngine().setSourceVelocity(clip->sourceId, m_velocity);
        audioSystem.getEngine().setSourceDirection(clip->sourceId, m_direction);
        audioSystem.getEngine().setSourceMaxDistance(clip->sourceId, m_maxDistance);
        audioSystem.getEngine().setSourceRolloffFactor(clip->sourceId, m_rolloffFactor);
        audioSystem.getEngine().setSourceReferenceDistance(clip->sourceId, m_referenceDistance);
    }

    void AudioComponent::syncPositionWithEntity() {
        // Get the entity this component belongs to
        if (auto entity = getEntity()) {
            // Try to get transform component
            if (auto transform = entity->getComponent<TransformComponent>()) {
                setPosition(transform->position);
                
                // Also sync velocity if transform provides it
                // This would require velocity tracking in TransformComponent
            }
        }
    }

    // AudioListenerComponent Implementation

    AudioListenerComponent::AudioListenerComponent()
        : m_active(false)
        , m_position(0.0f)
        , m_velocity(0.0f)
        , m_orientationAt(0.0f, 0.0f, -1.0f)
        , m_orientationUp(0.0f, 1.0f, 0.0f)
        , m_globalVolume(1.0f) {
    }

    AudioListenerComponent::~AudioListenerComponent() {
        if (m_active) {
            setActive(false);
        }
    }

    void AudioListenerComponent::setActive(bool active) {
        m_active = active;
        
        if (m_active) {
            // Sync current properties with audio system
            syncWithAudioSystem();
            std::cout << "AudioListener activated" << std::endl;
        } else {
            std::cout << "AudioListener deactivated" << std::endl;
        }
    }

    void AudioListenerComponent::setGlobalVolume(float volume) {
        m_globalVolume = std::max(0.0f, std::min(1.0f, volume));
        
        if (m_active) {
            auto& audioSystem = AudioSystem::getInstance();
            audioSystem.getEngine().setGlobalVolume(m_globalVolume);
        }
    }

    void AudioListenerComponent::setVelocity(const glm::vec3& velocity) {
        m_velocity = velocity;
        
        if (m_active) {
            auto& audioSystem = AudioSystem::getInstance();
            audioSystem.getEngine().setListenerVelocity(m_velocity);
        }
    }

    void AudioListenerComponent::update() {
        if (!m_active) {
            return;
        }

        // Sync position and orientation with entity's transform
        syncPositionWithEntity();
        
        // Update audio system
        syncWithAudioSystem();
    }

    void AudioListenerComponent::setPosition(const glm::vec3& position) {
        m_position = position;
        
        if (m_active) {
            auto& audioSystem = AudioSystem::getInstance();
            audioSystem.setListenerPosition(m_position);
        }
    }

    void AudioListenerComponent::setOrientation(const glm::vec3& at, const glm::vec3& up) {
        m_orientationAt = glm::normalize(at);
        m_orientationUp = glm::normalize(up);
        
        if (m_active) {
            auto& audioSystem = AudioSystem::getInstance();
            audioSystem.getEngine().setListenerOrientation(m_orientationAt, m_orientationUp);
        }
    }

    void AudioListenerComponent::syncWithAudioSystem() {
        if (!m_active) {
            return;
        }

        auto& audioSystem = AudioSystem::getInstance();
        audioSystem.setListenerPosition(m_position);
        audioSystem.getEngine().setListenerVelocity(m_velocity);
        audioSystem.getEngine().setListenerOrientation(m_orientationAt, m_orientationUp);
        audioSystem.getEngine().setGlobalVolume(m_globalVolume);
    }

    void AudioListenerComponent::syncPositionWithEntity() {
        // Get the entity this component belongs to
        if (auto entity = getEntity()) {
            // Try to get transform component
            if (auto transform = entity->getComponent<TransformComponent>()) {
                setPosition(transform->position);
                
                // For camera entities, we might also want to get orientation
                // This would depend on how camera orientation is stored
            }
        }
    }

} // namespace IKore
