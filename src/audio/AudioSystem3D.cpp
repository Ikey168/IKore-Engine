#include "AudioSystem3D.h"
#include "core/components/AudioComponent.h"
#include "core/Entity.h"
#include <iostream>
#include <algorithm>

namespace IKore {

    AudioSystem3D::AudioSystem3D() 
        : m_initialized(false)
        , m_updateTimer(0.0f)
        , m_frameCount(0) {
    }

    AudioSystem3D::~AudioSystem3D() {
        shutdown();
    }

    bool AudioSystem3D::initialize() {
        if (m_initialized) {
            return true;
        }

        std::cout << "Initializing 3D Audio System..." << std::endl;

        // Initialize the underlying OpenAL audio engine
        if (!AudioSystem::getInstance().initialize()) {
            std::cerr << "Failed to initialize OpenAL audio engine" << std::endl;
            return false;
        }

        // Set default 3D audio properties
        setDopplerFactor(1.0f);
        setSpeedOfSound(343.3f); // Speed of sound in air at 20°C
        setGlobalVolume(1.0f);

        m_initialized = true;
        std::cout << "3D Audio System initialized successfully!" << std::endl;
        
        return true;
    }

    void AudioSystem3D::shutdown() {
        if (!m_initialized) {
            return;
        }

        std::cout << "Shutting down 3D Audio System..." << std::endl;

        // Clear all entity references
        m_audioEntities.clear();
        m_listenerEntities.clear();
        m_activeListener.reset();

        // Shutdown the audio engine
        AudioSystem::getInstance().shutdown();

        m_initialized = false;
        std::cout << "3D Audio System shutdown complete." << std::endl;
    }

    void AudioSystem3D::registerAudioEntity(std::shared_ptr<Entity> entity) {
        if (!entity) {
            return;
        }

        // Check if entity has AudioComponent
        if (entity->getComponent<AudioComponent>()) {
            m_audioEntities.insert(entity);
            std::cout << "Registered audio entity (ID: " << entity->getID() << ")" << std::endl;
        } else {
            std::cerr << "Warning: Attempted to register entity without AudioComponent" << std::endl;
        }
    }

    void AudioSystem3D::unregisterAudioEntity(std::shared_ptr<Entity> entity) {
        if (!entity) {
            return;
        }

        auto it = std::find_if(m_audioEntities.begin(), m_audioEntities.end(),
            [entity](const std::weak_ptr<Entity>& weak_entity) {
                auto shared_entity = weak_entity.lock();
                return shared_entity && shared_entity == entity;
            });

        if (it != m_audioEntities.end()) {
            m_audioEntities.erase(it);
            std::cout << "Unregistered audio entity (ID: " << entity->getID() << ")" << std::endl;
        }
    }

    void AudioSystem3D::registerListenerEntity(std::shared_ptr<Entity> entity) {
        if (!entity) {
            return;
        }

        // Check if entity has AudioListenerComponent
        if (entity->getComponent<AudioListenerComponent>()) {
            m_listenerEntities.insert(entity);
            
            // If this is the first listener, make it active
            if (m_activeListener.expired()) {
                setActiveListener(entity);
            }
            
            std::cout << "Registered audio listener entity (ID: " << entity->getID() << ")" << std::endl;
        } else {
            std::cerr << "Warning: Attempted to register entity without AudioListenerComponent" << std::endl;
        }
    }

    void AudioSystem3D::unregisterListenerEntity(std::shared_ptr<Entity> entity) {
        if (!entity) {
            return;
        }

        auto it = std::find_if(m_listenerEntities.begin(), m_listenerEntities.end(),
            [entity](const std::weak_ptr<Entity>& weak_entity) {
                auto shared_entity = weak_entity.lock();
                return shared_entity && shared_entity == entity;
            });

        if (it != m_listenerEntities.end()) {
            m_listenerEntities.erase(it);
            
            // If this was the active listener, find another one
            if (auto activeListener = m_activeListener.lock()) {
                if (activeListener == entity) {
                    m_activeListener.reset();
                    
                    // Try to find another listener to activate
                    for (const auto& weak_entity : m_listenerEntities) {
                        if (auto listener_entity = weak_entity.lock()) {
                            setActiveListener(listener_entity);
                            break;
                        }
                    }
                }
            }
            
            std::cout << "Unregistered audio listener entity (ID: " << entity->getID() << ")" << std::endl;
        }
    }

    void AudioSystem3D::update(float deltaTime) {
        if (!m_initialized) {
            return;
        }

        m_updateTimer += deltaTime;
        m_frameCount++;

        // Update the audio engine
        AudioSystem::getInstance().update();

        // Clean up any dead entity references
        cleanupDeadEntities();

        // Update audio components
        updateAudioComponents();

        // Update listener components
        updateListenerComponents();

        // Print statistics every 5 seconds (for debugging)
        if (m_updateTimer >= 5.0f) {
            printAudioStatistics();
            m_updateTimer = 0.0f;
        }
    }

    void AudioSystem3D::setGlobalVolume(float volume) {
        if (!m_initialized) {
            return;
        }

        AudioSystem::getInstance().getEngine().setGlobalVolume(volume);
    }

    float AudioSystem3D::getGlobalVolume() const {
        if (!m_initialized) {
            return 0.0f;
        }

        return AudioSystem::getInstance().getEngine().getGlobalVolume();
    }

    void AudioSystem3D::setDopplerFactor(float factor) {
        if (!m_initialized) {
            return;
        }

        AudioSystem::getInstance().getEngine().setDopplerFactor(factor);
    }

    void AudioSystem3D::setSpeedOfSound(float speed) {
        if (!m_initialized) {
            return;
        }

        AudioSystem::getInstance().getEngine().setSpeedOfSound(speed);
    }

    void AudioSystem3D::setActiveListener(std::shared_ptr<Entity> listenerEntity) {
        if (!listenerEntity) {
            m_activeListener.reset();
            return;
        }

        auto listenerComponent = listenerEntity->getComponent<AudioListenerComponent>();
        if (!listenerComponent) {
            std::cerr << "Warning: Attempted to set active listener on entity without AudioListenerComponent" << std::endl;
            return;
        }

        // Deactivate current listener
        if (auto currentListener = m_activeListener.lock()) {
            if (auto currentComponent = currentListener->getComponent<AudioListenerComponent>()) {
                currentComponent->setActive(false);
            }
        }

        // Activate new listener
        m_activeListener = listenerEntity;
        listenerComponent->setActive(true);
        
        std::cout << "Set active audio listener to entity (ID: " << listenerEntity->getID() << ")" << std::endl;
    }

    uint32_t AudioSystem3D::playSound3D(const std::string& filename, const glm::vec3& position, 
                                       float volume, float pitch) {
        if (!m_initialized) {
            return 0;
        }

        auto& audioEngine = AudioSystem::getInstance().getEngine();
        
        // Load and play the sound
        uint32_t sourceId = audioEngine.loadSound(filename);
        if (sourceId == 0) {
            return 0;
        }

        // Configure 3D properties
        audioEngine.setSourcePosition(sourceId, position);
        audioEngine.setSourceGain(sourceId, volume);
        audioEngine.setSourcePitch(sourceId, pitch);
        
        // Play the sound
        audioEngine.playSound(sourceId);
        
        return sourceId;
    }

    uint32_t AudioSystem3D::playSound2D(const std::string& filename, float volume, float pitch) {
        if (!m_initialized) {
            return 0;
        }

        auto& audioEngine = AudioSystem::getInstance().getEngine();
        
        // Load and play the sound
        uint32_t sourceId = audioEngine.loadSound(filename);
        if (sourceId == 0) {
            return 0;
        }

        // Configure for 2D audio (no distance attenuation)
        audioEngine.setSourceGain(sourceId, volume);
        audioEngine.setSourcePitch(sourceId, pitch);
        audioEngine.setSourceRolloffFactor(sourceId, 0.0f); // No distance attenuation
        
        // Play the sound
        audioEngine.playSound(sourceId);
        
        return sourceId;
    }

    void AudioSystem3D::stopSound(uint32_t sourceId) {
        if (!m_initialized || sourceId == 0) {
            return;
        }

        AudioSystem::getInstance().getEngine().stopSound(sourceId);
    }

    size_t AudioSystem3D::getActiveAudioEntityCount() const {
        return m_audioEntities.size();
    }

    size_t AudioSystem3D::getActiveSoundSourceCount() const {
        if (!m_initialized) {
            return 0;
        }

        return AudioSystem::getInstance().getEngine().getActiveSourceCount();
    }

    std::vector<std::string> AudioSystem3D::getLoadedSounds() const {
        if (!m_initialized) {
            return {};
        }

        return AudioSystem::getInstance().getEngine().getLoadedSounds();
    }

    void AudioSystem3D::printAudioStatistics() const {
        if (!m_initialized) {
            return;
        }

        std::cout << "\n=== 3D Audio System Statistics ===" << std::endl;
        std::cout << "Registered Audio Entities: " << getActiveAudioEntityCount() << std::endl;
        std::cout << "Registered Listener Entities: " << m_listenerEntities.size() << std::endl;
        std::cout << "Active Sound Sources: " << getActiveSoundSourceCount() << std::endl;
        std::cout << "Loaded Audio Files: " << AudioSystem::getInstance().getEngine().getLoadedBufferCount() << std::endl;
        std::cout << "Global Volume: " << (getGlobalVolume() * 100.0f) << "%" << std::endl;
        
        if (auto activeListener = m_activeListener.lock()) {
            auto listenerPos = AudioSystem::getInstance().getEngine().getListenerPosition();
            std::cout << "Active Listener Position: (" << listenerPos.x << ", " << listenerPos.y << ", " << listenerPos.z << ")" << std::endl;
        } else {
            std::cout << "No Active Listener" << std::endl;
        }
        
        std::cout << "================================\n" << std::endl;
    }

    // Private helper methods

    void AudioSystem3D::updateAudioComponents() {
        for (const auto& weak_entity : m_audioEntities) {
            if (auto entity = weak_entity.lock()) {
                if (auto audioComponent = entity->getComponent<AudioComponent>()) {
                    audioComponent->update();
                }
            }
        }
    }

    void AudioSystem3D::updateListenerComponents() {
        for (const auto& weak_entity : m_listenerEntities) {
            if (auto entity = weak_entity.lock()) {
                if (auto listenerComponent = entity->getComponent<AudioListenerComponent>()) {
                    listenerComponent->update();
                }
            }
        }
    }

    void AudioSystem3D::cleanupDeadEntities() {
        // Clean up audio entities
        auto audio_it = m_audioEntities.begin();
        while (audio_it != m_audioEntities.end()) {
            if (!isEntityValid(*audio_it)) {
                audio_it = m_audioEntities.erase(audio_it);
            } else {
                ++audio_it;
            }
        }

        // Clean up listener entities
        auto listener_it = m_listenerEntities.begin();
        while (listener_it != m_listenerEntities.end()) {
            if (!isEntityValid(*listener_it)) {
                // If this was the active listener, clear it
                if (auto activeListener = m_activeListener.lock()) {
                    if (auto entity = listener_it->lock()) {
                        if (entity == activeListener) {
                            m_activeListener.reset();
                        }
                    }
                }
                listener_it = m_listenerEntities.erase(listener_it);
            } else {
                ++listener_it;
            }
        }

        // Clean up active listener if it's dead
        if (!isEntityValid(m_activeListener)) {
            m_activeListener.reset();
        }
    }

    bool AudioSystem3D::isEntityValid(const std::weak_ptr<Entity>& entity) const {
        return !entity.expired();
    }

    // AudioManager Implementation

    AudioManager& AudioManager::getInstance() {
        static AudioManager instance;
        return instance;
    }

    void AudioManager::playSound(const std::string& filename, const glm::vec3& position) {
        auto& audioEngine = AudioSystem::getInstance().getEngine();
        
        uint32_t sourceId = audioEngine.loadSound(filename);
        if (sourceId != 0) {
            audioEngine.setSourcePosition(sourceId, position);
            audioEngine.playSound(sourceId);
        }
    }

    void AudioManager::playMusic(const std::string& filename, bool loop) {
        auto& instance = getInstance();
        auto& audioEngine = AudioSystem::getInstance().getEngine();
        
        // Stop current music
        if (instance.m_musicSourceId != 0) {
            stopMusic();
        }
        
        // Load and play new music
        instance.m_musicSourceId = audioEngine.loadStreamingSound(filename);
        if (instance.m_musicSourceId != 0) {
            audioEngine.setSourceLooping(instance.m_musicSourceId, loop);
            audioEngine.setSourceGain(instance.m_musicSourceId, instance.m_musicVolume);
            audioEngine.setSourceRolloffFactor(instance.m_musicSourceId, 0.0f); // No distance attenuation for music
            audioEngine.playSound(instance.m_musicSourceId);
        }
    }

    void AudioManager::stopMusic() {
        auto& instance = getInstance();
        auto& audioEngine = AudioSystem::getInstance().getEngine();
        
        if (instance.m_musicSourceId != 0) {
            audioEngine.stopSound(instance.m_musicSourceId);
            audioEngine.unloadSound(instance.m_musicSourceId);
            instance.m_musicSourceId = 0;
        }
    }

    void AudioManager::setMusicVolume(float volume) {
        auto& instance = getInstance();
        instance.m_musicVolume = std::max(0.0f, std::min(1.0f, volume));
        
        if (instance.m_musicSourceId != 0) {
            auto& audioEngine = AudioSystem::getInstance().getEngine();
            audioEngine.setSourceGain(instance.m_musicSourceId, instance.m_musicVolume);
        }
    }

    void AudioManager::setSFXVolume(float volume) {
        auto& instance = getInstance();
        instance.m_sfxVolume = std::max(0.0f, std::min(1.0f, volume));
        
        // Set global volume for sound effects
        AudioSystem::getInstance().getEngine().setGlobalVolume(instance.m_sfxVolume);
    }

    void AudioManager::pauseAll() {
        auto& instance = getInstance();
        if (!instance.m_paused) {
            // In a full implementation, we would pause all active sources
            // For now, we'll just set a flag
            instance.m_paused = true;
        }
    }

    void AudioManager::resumeAll() {
        auto& instance = getInstance();
        if (instance.m_paused) {
            // In a full implementation, we would resume all paused sources
            instance.m_paused = false;
        }
    }

    void AudioManager::stopAll() {
        auto& audioEngine = AudioSystem::getInstance().getEngine();
        audioEngine.stopAllSounds();
        stopMusic();
    }

} // namespace IKore
