#include "OpenALAudioEngine.h"
#include "AudioDecode.h"
#include "AudioDecodeCompressed.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <cstring>
#include <algorithm>
#include <cmath>

// Include audio format libraries (these would need to be added to CMake)
// For now, we'll implement basic WAV support and stubs for others
#ifdef HAVE_VORBIS
#include <vorbis/vorbisfile.h>
#endif

#ifdef HAVE_LIBSNDFILE
#include <sndfile.h>
#endif

namespace IKore {

namespace {
    // Streaming ring parameters (issue #258): four buffers of ~1/3 second at
    // 44.1 kHz stereo keeps latency low with headroom against underruns.
    constexpr int kStreamBufferCount = 4;
    constexpr std::size_t kStreamChunkBytes = 65536;
} // namespace

#if OPENAL_AVAILABLE
    // Full OpenAL implementation when available
    OpenALAudioEngine::OpenALAudioEngine() 
        : m_device(nullptr)
        , m_context(nullptr)
        , m_initialized(false)
        , m_nextSourceId(1)
        , m_maxConcurrentSources(32)
        , m_globalVolume(1.0f)
        , m_streamingActive(false) {
        
        m_listener = std::make_unique<AudioListener>();
    }

    OpenALAudioEngine::~OpenALAudioEngine() {
        shutdown();
    }

    bool OpenALAudioEngine::initialize() {
        if (m_initialized) {
            return true;
        }

        std::cout << "Initializing OpenAL 3D Audio System..." << std::endl;

        // Open the default audio device
        m_device = alcOpenDevice(nullptr);
        if (!m_device) {
            m_lastError = "Failed to open OpenAL device";
            std::cerr << "OpenAL Error: " << m_lastError << std::endl;
            return false;
        }

        // Create audio context
        m_context = alcCreateContext(m_device, nullptr);
        if (!m_context) {
            m_lastError = "Failed to create OpenAL context";
            std::cerr << "OpenAL Error: " << m_lastError << std::endl;
            alcCloseDevice(m_device);
            m_device = nullptr;
            return false;
        }

        // Make context current
        if (!alcMakeContextCurrent(m_context)) {
            m_lastError = "Failed to make OpenAL context current";
            std::cerr << "OpenAL Error: " << m_lastError << std::endl;
            alcDestroyContext(m_context);
            alcCloseDevice(m_device);
            m_context = nullptr;
            m_device = nullptr;
            return false;
        }

        // Check for errors
        if (!checkALError("Initialize OpenAL context")) {
            shutdown();
            return false;
        }

        // Set up distance model for 3D audio
        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
        
        // Initialize listener
        setListenerPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        setListenerVelocity(glm::vec3(0.0f, 0.0f, 0.0f));
        setListenerOrientation(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        setListenerGain(m_globalVolume);

        // Start streaming thread for background audio processing
        m_streamingActive = true;
        m_streamingThread = std::thread(&OpenALAudioEngine::streamingThreadFunction, this);

        m_initialized = true;
        
        std::cout << "OpenAL 3D Audio System initialized successfully!" << std::endl;
        printAudioDeviceInfo();
        
        return true;
    }

    void OpenALAudioEngine::shutdown() {
        if (!m_initialized) {
            return;
        }

        std::cout << "Shutting down OpenAL 3D Audio System..." << std::endl;

        // Stop streaming thread
        m_streamingActive = false;
        if (m_streamingThread.joinable()) {
            m_streamingThread.join();
        }

        // Stop all sounds and clean up sources
        stopAllSounds();
        unloadAllSounds();

        // Clean up OpenAL context and device
        if (m_context) {
            alcMakeContextCurrent(nullptr);
            alcDestroyContext(m_context);
            m_context = nullptr;
        }

        if (m_device) {
            alcCloseDevice(m_device);
            m_device = nullptr;
        }

        m_initialized = false;
        std::cout << "OpenAL 3D Audio System shutdown complete." << std::endl;
    }

    uint32_t OpenALAudioEngine::loadSound(const std::string& filename) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        if (!m_initialized) {
            m_lastError = "Audio engine not initialized";
            return 0;
        }

        // Check if buffer already exists
        auto bufferIt = m_audioBuffers.find(filename);
        AudioBuffer* buffer = nullptr;
        
        if (bufferIt != m_audioBuffers.end()) {
            buffer = bufferIt->second.get();
        } else {
            // Load new audio file
            auto newBuffer = std::make_unique<AudioBuffer>();
            if (!loadAudioFile(filename, *newBuffer)) {
                m_lastError = "Failed to load audio file: " + filename;
                return 0;
            }
            
            buffer = newBuffer.get();
            m_audioBuffers[filename] = std::move(newBuffer);
        }

        // Create new audio source. Insert it into the map BEFORE createAudioSource:
        // that call looks the source up by id, so the old order (create first,
        // insert after) made it fail unconditionally.
        uint32_t sourceId = m_nextSourceId++;
        auto audioSource = std::make_unique<AudioSource>();
        audioSource->bufferId = buffer->bufferId;
        audioSource->audioFile = filename;
        m_audioSources[sourceId] = std::move(audioSource);

        if (!createAudioSource(sourceId)) {
            m_audioSources.erase(sourceId);
            m_lastError = "Failed to create audio source for: " + filename;
            return 0;
        }

        // Configure source with buffer
        AudioSource* source = m_audioSources[sourceId].get();
        alSourcei(source->sourceId, AL_BUFFER, buffer->bufferId);

        if (!checkALError("Bind buffer to source")) {
            cleanupSource(sourceId);
            m_audioSources.erase(sourceId);
            return 0;
        }

        std::cout << "Loaded 3D audio: " << filename << " (Source ID: " << sourceId << ")" << std::endl;
        return sourceId;
    }

    uint32_t OpenALAudioEngine::loadStreamingSound(const std::string& filename) {
        std::lock_guard<std::mutex> lock(m_audioMutex);

        if (!m_initialized) {
            m_lastError = "Audio engine not initialized";
            return 0;
        }

        // Decode the whole file to PCM up front (issue #258); the source then
        // streams it through a small ring of buffers instead of one static buffer,
        // so playback starts immediately and memory stays bounded per chunk on the
        // device side. A chunked decoder can later replace the full decode without
        // touching the queue logic.
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            m_lastError = "Could not open audio file: " + filename;
            return 0;
        }
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),
                                         std::istreambuf_iterator<char>());
        file.close();

        audio::PcmAudio pcm;
        std::string decodeErr;
        if (!audio::decodeAudio(filename, bytes.data(), bytes.size(), pcm, decodeErr) || !pcm.valid()) {
            m_lastError = "Failed to decode " + filename + ": " + decodeErr;
            return 0;
        }

        const uint32_t sourceId = m_nextSourceId++;
        auto audioSource = std::make_unique<AudioSource>();
        audioSource->audioFile = filename;
        audioSource->streaming = true;
        audioSource->streamPcm = std::move(pcm);
        m_audioSources[sourceId] = std::move(audioSource);

        if (!createAudioSource(sourceId)) {
            m_audioSources.erase(sourceId);
            m_lastError = "Failed to create audio source for: " + filename;
            return 0;
        }

        // Prime the ring: generate the stream buffers, fill each with the next
        // chunk, and queue them on the source.
        AudioSource* source = m_audioSources[sourceId].get();
        source->streamBuffers.assign(kStreamBufferCount, 0);
        alGenBuffers(static_cast<ALsizei>(source->streamBuffers.size()), source->streamBuffers.data());
        if (!checkALError("Generate stream buffers")) {
            cleanupSource(sourceId);
            m_audioSources.erase(sourceId);
            return 0;
        }
        for (ALuint buf : source->streamBuffers) {
            if (!fillStreamBuffer(*source, buf)) break; // stream shorter than the ring
            alSourceQueueBuffers(source->sourceId, 1, &buf);
        }
        if (!checkALError("Queue stream buffers")) {
            cleanupSource(sourceId);
            m_audioSources.erase(sourceId);
            return 0;
        }

        m_streamingSources.push_back(sourceId);
        std::cout << "Loaded streaming audio: " << filename << " (Source ID: " << sourceId << ")" << std::endl;
        return sourceId;
    }

    void OpenALAudioEngine::unloadSound(uint32_t sourceId) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it != m_audioSources.end()) {
            stopSound(sourceId);
            cleanupSource(sourceId);
            m_audioSources.erase(it);
            
            // Remove from streaming sources if applicable
            auto streamIt = std::find(m_streamingSources.begin(), m_streamingSources.end(), sourceId);
            if (streamIt != m_streamingSources.end()) {
                m_streamingSources.erase(streamIt);
            }
        }
    }

    void OpenALAudioEngine::unloadAllSounds() {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        // Stop and cleanup all sources
        for (auto& pair : m_audioSources) {
            stopSound(pair.first);
            cleanupSource(pair.first);
        }
        m_audioSources.clear();
        m_streamingSources.clear();

        // Clean up all buffers
        for (auto& pair : m_audioBuffers) {
            if (pair.second->isLoaded) {
                alDeleteBuffers(1, &pair.second->bufferId);
            }
        }
        m_audioBuffers.clear();
    }

    void OpenALAudioEngine::playSound(uint32_t sourceId) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            m_lastError = "Invalid source ID: " + std::to_string(sourceId);
            return;
        }

        AudioSource* source = it->second.get();
        alSourcePlay(source->sourceId);
        source->isPlaying = true;
        source->isPaused = false;
        
        checkALError("Play sound");
    }

    void OpenALAudioEngine::pauseSound(uint32_t sourceId) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return;
        }

        AudioSource* source = it->second.get();
        alSourcePause(source->sourceId);
        source->isPaused = true;
        
        checkALError("Pause sound");
    }

    void OpenALAudioEngine::stopSound(uint32_t sourceId) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return;
        }

        AudioSource* source = it->second.get();
        alSourceStop(source->sourceId);
        source->isPlaying = false;
        source->isPaused = false;
        
        checkALError("Stop sound");
    }

    void OpenALAudioEngine::stopAllSounds() {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        for (auto& pair : m_audioSources) {
            alSourceStop(pair.second->sourceId);
            pair.second->isPlaying = false;
            pair.second->isPaused = false;
        }
    }

    void OpenALAudioEngine::setSourcePosition(uint32_t sourceId, const glm::vec3& position) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return;
        }

        AudioSource* source = it->second.get();
        source->position = position;
        
        if (source->is3D) {
            alSource3f(source->sourceId, AL_POSITION, position.x, position.y, position.z);
            checkALError("Set source position");
        }
    }

    void OpenALAudioEngine::setSourceVelocity(uint32_t sourceId, const glm::vec3& velocity) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return;
        }

        AudioSource* source = it->second.get();
        source->velocity = velocity;
        
        if (source->is3D) {
            alSource3f(source->sourceId, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
            checkALError("Set source velocity");
        }
    }

    void OpenALAudioEngine::setSourceDirection(uint32_t sourceId, const glm::vec3& direction) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return;
        }

        AudioSource* source = it->second.get();
        source->direction = direction;
        
        if (source->is3D) {
            alSource3f(source->sourceId, AL_DIRECTION, direction.x, direction.y, direction.z);
            checkALError("Set source direction");
        }
    }

    glm::vec3 OpenALAudioEngine::getSourcePosition(uint32_t sourceId) const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_audioMutex));
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return glm::vec3(0.0f);
        }

        return it->second->position;
    }

    void OpenALAudioEngine::setSourceGain(uint32_t sourceId, float gain) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return;
        }

        AudioSource* source = it->second.get();
        source->gain = std::max(0.0f, gain);
        
        alSourcef(source->sourceId, AL_GAIN, source->gain);
        checkALError("Set source gain");
    }

    void OpenALAudioEngine::setSourcePitch(uint32_t sourceId, float pitch) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return;
        }

        AudioSource* source = it->second.get();
        source->pitch = std::max(0.01f, pitch); // Prevent zero pitch
        
        alSourcef(source->sourceId, AL_PITCH, source->pitch);
        checkALError("Set source pitch");
    }

    void OpenALAudioEngine::setSourceLooping(uint32_t sourceId, bool looping) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return;
        }

        AudioSource* source = it->second.get();
        source->looping = looping;
        
        alSourcei(source->sourceId, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
        checkALError("Set source looping");
    }

    void OpenALAudioEngine::setSourceRolloffFactor(uint32_t sourceId, float rolloff) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return;
        }

        AudioSource* source = it->second.get();
        source->rolloffFactor = std::max(0.0f, rolloff);
        
        alSourcef(source->sourceId, AL_ROLLOFF_FACTOR, source->rolloffFactor);
        checkALError("Set source rolloff factor");
    }

    void OpenALAudioEngine::setSourceMaxDistance(uint32_t sourceId, float maxDistance) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return;
        }

        AudioSource* source = it->second.get();
        source->maxDistance = std::max(0.1f, maxDistance);
        
        alSourcef(source->sourceId, AL_MAX_DISTANCE, source->maxDistance);
        checkALError("Set source max distance");
    }

    void OpenALAudioEngine::setSourceReferenceDistance(uint32_t sourceId, float refDistance) {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return;
        }

        AudioSource* source = it->second.get();
        source->referenceDistance = std::max(0.1f, refDistance);
        
        alSourcef(source->sourceId, AL_REFERENCE_DISTANCE, source->referenceDistance);
        checkALError("Set source reference distance");
    }

    void OpenALAudioEngine::setListenerPosition(const glm::vec3& position) {
        if (!m_initialized) return;
        
        m_listener->position = position;
        alListener3f(AL_POSITION, position.x, position.y, position.z);
        checkALError("Set listener position");
    }

    void OpenALAudioEngine::setListenerVelocity(const glm::vec3& velocity) {
        if (!m_initialized) return;
        
        m_listener->velocity = velocity;
        alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
        checkALError("Set listener velocity");
    }

    void OpenALAudioEngine::setListenerOrientation(const glm::vec3& at, const glm::vec3& up) {
        if (!m_initialized) return;
        
        m_listener->orientation[0] = at;
        m_listener->orientation[1] = up;
        
        float orientation[6] = {
            at.x, at.y, at.z,
            up.x, up.y, up.z
        };
        
        alListenerfv(AL_ORIENTATION, orientation);
        checkALError("Set listener orientation");
    }

    void OpenALAudioEngine::setListenerGain(float gain) {
        if (!m_initialized) return;
        
        m_listener->gain = std::max(0.0f, gain);
        alListenerf(AL_GAIN, m_listener->gain);
        checkALError("Set listener gain");
    }

    glm::vec3 OpenALAudioEngine::getListenerPosition() const {
        return m_listener->position;
    }

    bool OpenALAudioEngine::isPlaying(uint32_t sourceId) const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_audioMutex));
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return false;
        }

        ALint state;
        alGetSourcei(it->second->sourceId, AL_SOURCE_STATE, &state);
        return state == AL_PLAYING;
    }

    bool OpenALAudioEngine::isPaused(uint32_t sourceId) const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_audioMutex));
        
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return false;
        }

        ALint state;
        alGetSourcei(it->second->sourceId, AL_SOURCE_STATE, &state);
        return state == AL_PAUSED;
    }

    void OpenALAudioEngine::update() {
        if (!m_initialized) return;
        
        std::lock_guard<std::mutex> lock(m_audioMutex);
        
        // Update source states
        for (auto& pair : m_audioSources) {
            AudioSource* source = pair.second.get();
            
            ALint state;
            alGetSourcei(source->sourceId, AL_SOURCE_STATE, &state);
            
            source->isPlaying = (state == AL_PLAYING);
            source->isPaused = (state == AL_PAUSED);
        }
    }

    size_t OpenALAudioEngine::getActiveSourceCount() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_audioMutex));
        
        size_t activeCount = 0;
        for (const auto& pair : m_audioSources) {
            if (pair.second->isPlaying) {
                activeCount++;
            }
        }
        return activeCount;
    }

    size_t OpenALAudioEngine::getLoadedBufferCount() const {
        return m_audioBuffers.size();
    }

    std::vector<std::string> OpenALAudioEngine::getLoadedSounds() const {
        std::vector<std::string> sounds;
        sounds.reserve(m_audioBuffers.size());
        
        for (const auto& pair : m_audioBuffers) {
            sounds.push_back(pair.first);
        }
        
        return sounds;
    }

    void OpenALAudioEngine::setGlobalVolume(float volume) {
        m_globalVolume = std::max(0.0f, std::min(1.0f, volume));
        setListenerGain(m_globalVolume);
    }

    float OpenALAudioEngine::getGlobalVolume() const {
        return m_globalVolume;
    }

    void OpenALAudioEngine::setDopplerFactor(float factor) {
        if (!m_initialized) return;
        
        alDopplerFactor(std::max(0.0f, factor));
        checkALError("Set Doppler factor");
    }

    void OpenALAudioEngine::setSpeedOfSound(float speed) {
        if (!m_initialized) return;
        
        alSpeedOfSound(std::max(0.1f, speed));
        checkALError("Set speed of sound");
    }

    std::string OpenALAudioEngine::getLastError() const {
        return m_lastError;
    }

    bool OpenALAudioEngine::checkALError(const std::string& operation) {
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            logALError(error, operation);
            return false;
        }
        return true;
    }

    void OpenALAudioEngine::printAudioDeviceInfo() const {
        if (!m_initialized) return;
        
        std::cout << "\n=== OpenAL Audio Device Information ===" << std::endl;
        std::cout << "Device: " << alcGetString(m_device, ALC_DEVICE_SPECIFIER) << std::endl;
        std::cout << "Vendor: " << alGetString(AL_VENDOR) << std::endl;
        std::cout << "Version: " << alGetString(AL_VERSION) << std::endl;
        std::cout << "Renderer: " << alGetString(AL_RENDERER) << std::endl;
        std::cout << "Extensions: " << alGetString(AL_EXTENSIONS) << std::endl;
        
        ALCint frequency, refresh;
        alcGetIntegerv(m_device, ALC_FREQUENCY, 1, &frequency);
        alcGetIntegerv(m_device, ALC_REFRESH, 1, &refresh);
        
        std::cout << "Frequency: " << frequency << " Hz" << std::endl;
        std::cout << "Refresh Rate: " << refresh << " Hz" << std::endl;
        std::cout << "Max Concurrent Sources: " << m_maxConcurrentSources << std::endl;
        std::cout << "========================================\n" << std::endl;
    }

    // Private helper methods implementation

    bool OpenALAudioEngine::createAudioSource(uint32_t sourceId) {
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) {
            return false;
        }

        AudioSource* source = it->second.get();
        
        // Generate OpenAL source
        alGenSources(1, &source->sourceId);
        if (!checkALError("Generate audio source")) {
            return false;
        }

        // Set default 3D properties
        alSourcef(source->sourceId, AL_GAIN, source->gain);
        alSourcef(source->sourceId, AL_PITCH, source->pitch);
        alSource3f(source->sourceId, AL_POSITION, source->position.x, source->position.y, source->position.z);
        alSource3f(source->sourceId, AL_VELOCITY, source->velocity.x, source->velocity.y, source->velocity.z);
        alSource3f(source->sourceId, AL_DIRECTION, source->direction.x, source->direction.y, source->direction.z);
        
        alSourcef(source->sourceId, AL_ROLLOFF_FACTOR, source->rolloffFactor);
        alSourcef(source->sourceId, AL_MAX_DISTANCE, source->maxDistance);
        alSourcef(source->sourceId, AL_REFERENCE_DISTANCE, source->referenceDistance);
        alSourcei(source->sourceId, AL_LOOPING, source->looping ? AL_TRUE : AL_FALSE);

        return checkALError("Configure audio source");
    }

    bool OpenALAudioEngine::loadAudioFile(const std::string& filename, AudioBuffer& buffer) {
        buffer.filename = filename;
        
        // Determine file format and load accordingly
        std::string extension = filename.substr(filename.find_last_of('.') + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        
        bool success = false;
        
        if (extension == "wav") {
            success = loadWAV(filename, buffer);
        } else if (extension == "ogg") {
            success = loadOGG(filename, buffer);
        } else if (extension == "mp3") {
            success = loadMP3(filename, buffer);
        } else {
            m_lastError = "Unsupported audio format: " + extension;
            return false;
        }
        
        if (success) {
            buffer.isLoaded = true;
        }
        
        return success;
    }

    void OpenALAudioEngine::cleanupSource(uint32_t sourceId) {
        auto it = m_audioSources.find(sourceId);
        if (it == m_audioSources.end()) return;
        if (it->second->sourceId != 0) {
            alDeleteSources(1, &it->second->sourceId);
            it->second->sourceId = 0;
        }
        // Release this source's streaming ring (deleting the source above detaches
        // any still-queued buffers, so they are safe to delete here).
        if (!it->second->streamBuffers.empty()) {
            alDeleteBuffers(static_cast<ALsizei>(it->second->streamBuffers.size()),
                            it->second->streamBuffers.data());
            it->second->streamBuffers.clear();
        }
    }

    void OpenALAudioEngine::streamingThreadFunction() {
        while (m_streamingActive) {
            updateStreamingSources();
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
        }
    }

    bool OpenALAudioEngine::fillStreamBuffer(AudioSource& source, ALuint bufferId) {
        const audio::PcmAudio& pcm = source.streamPcm;
        const std::size_t bytesPerFrame = pcm.bytesPerFrame();
        if (bytesPerFrame == 0 || pcm.data.empty()) return false;

        // Wrap for looping sources; otherwise the stream ends when the cursor does.
        if (source.streamCursor >= pcm.data.size()) {
            if (!source.looping) {
                source.streamEnded = true;
                return false;
            }
            source.streamCursor = 0;
        }

        // Next chunk, clipped to the end of the PCM and to whole frames.
        std::size_t take = pcm.data.size() - source.streamCursor;
        if (take > kStreamChunkBytes) take = kStreamChunkBytes;
        take -= take % bytesPerFrame;
        if (take == 0) {
            source.streamEnded = !source.looping;
            return false;
        }

        const ALenum format = (pcm.channels == 2)
            ? (pcm.bitsPerSample == 8 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16)
            : (pcm.bitsPerSample == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16);
        alBufferData(bufferId, format, pcm.data.data() + source.streamCursor,
                     static_cast<ALsizei>(take), pcm.sampleRate);
        source.streamCursor += take;
        return checkALError("Fill stream buffer");
    }

    void OpenALAudioEngine::updateStreamingSources() {
        std::lock_guard<std::mutex> lock(m_audioMutex);

        // Real buffer recycling (issue #258): unqueue the buffers OpenAL reports as
        // played and refill them with the next PCM chunk, wrapping when looping.
        for (uint32_t sourceId : m_streamingSources) {
            auto it = m_audioSources.find(sourceId);
            if (it == m_audioSources.end()) continue;
            AudioSource* source = it->second.get();
            if (source->streamBuffers.empty() || source->streamPcm.data.empty()) continue;

            ALint processed = 0;
            alGetSourcei(source->sourceId, AL_BUFFERS_PROCESSED, &processed);
            while (processed-- > 0) {
                ALuint buf = 0;
                alSourceUnqueueBuffers(source->sourceId, 1, &buf);
                if (alGetError() != AL_NO_ERROR) break;
                if (!fillStreamBuffer(*source, buf)) continue; // drained (non-looping)
                alSourceQueueBuffers(source->sourceId, 1, &buf);
            }

            // Recover from an underrun: still marked playing with data queued but
            // the device ran dry before we refilled - restart it.
            if (source->isPlaying && !source->isPaused) {
                ALint state = 0, queued = 0;
                alGetSourcei(source->sourceId, AL_SOURCE_STATE, &state);
                alGetSourcei(source->sourceId, AL_BUFFERS_QUEUED, &queued);
                if (state == AL_STOPPED) {
                    if (queued > 0) {
                        alSourcePlay(source->sourceId);
                    } else if (source->streamEnded) {
                        source->isPlaying = false; // finished naturally
                    }
                }
            }
        }
    }

    // Basic WAV loader implementation
    bool OpenALAudioEngine::loadWAV(const std::string& filename, AudioBuffer& buffer) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            m_lastError = "Could not open WAV file: " + filename;
            return false;
        }

        // Robust RIFF/WAVE parse via the shared decoder (issue #258): walks the
        // chunk list, so files with extra chunks or an extended fmt chunk load, not
        // just the canonical 44-byte layout the old inline parser assumed.
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),
                                         std::istreambuf_iterator<char>());
        file.close();

        audio::PcmAudio pcm;
        std::string decodeErr;
        if (!audio::parseWav(bytes.data(), bytes.size(), pcm, decodeErr)) {
            m_lastError = "Invalid WAV file (" + filename + "): " + decodeErr;
            return false;
        }

        if (pcm.channels == 1) {
            buffer.format = (pcm.bitsPerSample == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
        } else {
            buffer.format = (pcm.bitsPerSample == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
        }
        buffer.frequency = pcm.sampleRate;
        buffer.size = static_cast<ALsizei>(pcm.data.size());

        // Generate OpenAL buffer
        alGenBuffers(1, &buffer.bufferId);
        if (!checkALError("Generate audio buffer")) {
            return false;
        }

        // Upload audio data to buffer
        alBufferData(buffer.bufferId, buffer.format, pcm.data.data(), buffer.size, buffer.frequency);

        return checkALError("Upload audio data to buffer");
    }

    namespace {
        // Shared tail for the compressed loaders: upload decoded PCM to a new AL
        // buffer (issue #258).
        bool uploadPcmToBuffer(const audio::PcmAudio& pcm, AudioBuffer& buffer) {
            buffer.format = (pcm.channels == 2)
                ? (pcm.bitsPerSample == 8 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16)
                : (pcm.bitsPerSample == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16);
            buffer.frequency = pcm.sampleRate;
            buffer.size = static_cast<ALsizei>(pcm.data.size());
            alGenBuffers(1, &buffer.bufferId);
            if (alGetError() != AL_NO_ERROR) return false;
            alBufferData(buffer.bufferId, buffer.format, pcm.data.data(), buffer.size, buffer.frequency);
            return alGetError() == AL_NO_ERROR;
        }

        bool readFileBytes(const std::string& filename, std::vector<unsigned char>& out) {
            std::ifstream file(filename, std::ios::binary);
            if (!file.is_open()) return false;
            out.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return true;
        }
    } // namespace

    bool OpenALAudioEngine::loadOGG(const std::string& filename, AudioBuffer& buffer) {
        // OGG Vorbis decode via the vendored stb_vorbis (issue #258).
        std::vector<unsigned char> bytes;
        if (!readFileBytes(filename, bytes)) {
            m_lastError = "Could not open OGG file: " + filename;
            return false;
        }
        audio::PcmAudio pcm;
        std::string err;
        if (!audio::decodeOgg(bytes.data(), bytes.size(), pcm, err)) {
            m_lastError = "Invalid OGG file (" + filename + "): " + err;
            return false;
        }
        if (!uploadPcmToBuffer(pcm, buffer)) {
            m_lastError = "Failed to upload OGG audio to buffer: " + filename;
            return false;
        }
        return true;
    }

    bool OpenALAudioEngine::loadMP3(const std::string& filename, AudioBuffer& buffer) {
        // MP3 decode via the vendored dr_mp3 (issue #258).
        std::vector<unsigned char> bytes;
        if (!readFileBytes(filename, bytes)) {
            m_lastError = "Could not open MP3 file: " + filename;
            return false;
        }
        audio::PcmAudio pcm;
        std::string err;
        if (!audio::decodeMp3(bytes.data(), bytes.size(), pcm, err)) {
            m_lastError = "Invalid MP3 file (" + filename + "): " + err;
            return false;
        }
        if (!uploadPcmToBuffer(pcm, buffer)) {
            m_lastError = "Failed to upload MP3 audio to buffer: " + filename;
            return false;
        }
        return true;
    }

    void OpenALAudioEngine::logALError(ALenum error, const std::string& operation) const {
        std::string errorString;
        
        switch (error) {
            case AL_INVALID_NAME:
                errorString = "AL_INVALID_NAME";
                break;
            case AL_INVALID_ENUM:
                errorString = "AL_INVALID_ENUM";
                break;
            case AL_INVALID_VALUE:
                errorString = "AL_INVALID_VALUE";
                break;
            case AL_INVALID_OPERATION:
                errorString = "AL_INVALID_OPERATION";
                break;
            case AL_OUT_OF_MEMORY:
                errorString = "AL_OUT_OF_MEMORY";
                break;
            default:
                errorString = "Unknown OpenAL error";
                break;
        }
        
        std::cerr << "OpenAL Error (" << errorString << ") in operation: " << operation << std::endl;
    }

    void OpenALAudioEngine::logALCError(ALCdevice* device, const std::string& operation) const {
        ALCenum error = alcGetError(device);
        if (error != ALC_NO_ERROR) {
            std::string errorString;
            
            switch (error) {
                case ALC_INVALID_VALUE:
                    errorString = "ALC_INVALID_VALUE";
                    break;
                case ALC_INVALID_DEVICE:
                    errorString = "ALC_INVALID_DEVICE";
                    break;
                case ALC_INVALID_CONTEXT:
                    errorString = "ALC_INVALID_CONTEXT";
                    break;
                case ALC_INVALID_ENUM:
                    errorString = "ALC_INVALID_ENUM";
                    break;
                case ALC_OUT_OF_MEMORY:
                    errorString = "ALC_OUT_OF_MEMORY";
                    break;
                default:
                    errorString = "Unknown OpenAL Context error";
                    break;
            }
            
            std::cerr << "OpenAL Context Error (" << errorString << ") in operation: " << operation << std::endl;
        }
    }

#else
    // Fallback implementation when OpenAL is not available
    
    OpenALAudioEngine::OpenALAudioEngine() 
        : m_device(nullptr), m_context(nullptr), m_initialized(false), 
          m_nextSourceId(1), m_maxConcurrentSources(32), m_globalVolume(1.0f), 
          m_streamingActive(false) {
        m_listener = std::make_unique<AudioListener>();
        std::cout << "OpenAL Audio Engine: Fallback mode (OpenAL not available)" << std::endl;
    }

    OpenALAudioEngine::~OpenALAudioEngine() {
        shutdown();
    }

    bool OpenALAudioEngine::initialize() {
        std::cout << "OpenAL Audio Engine: Initialize called (OpenAL not available - using fallback)" << std::endl;
        m_initialized = true;
        return true;
    }

    void OpenALAudioEngine::shutdown() {
        if (m_initialized) {
            std::cout << "OpenAL Audio Engine: Shutdown called (fallback mode)" << std::endl;
            m_initialized = false;
        }
    }

    uint32_t OpenALAudioEngine::loadSound(const std::string& filename) {
        std::cout << "OpenAL Audio Engine: loadSound(" << filename << ") - fallback mode, returning dummy ID" << std::endl;
        return m_nextSourceId++;
    }

    uint32_t OpenALAudioEngine::loadStreamingSound(const std::string& filename) {
        std::cout << "OpenAL Audio Engine: loadStreamingSound(" << filename << ") - fallback mode" << std::endl;
        return m_nextSourceId++;
    }

    void OpenALAudioEngine::unloadSound(uint32_t sourceId) {
        std::cout << "OpenAL Audio Engine: unloadSound(" << sourceId << ") - fallback mode" << std::endl;
    }

    void OpenALAudioEngine::unloadAllSounds() {
        std::cout << "OpenAL Audio Engine: unloadAllSounds() - fallback mode" << std::endl;
    }

    void OpenALAudioEngine::playSound(uint32_t sourceId) {
        std::cout << "OpenAL Audio Engine: playSound(" << sourceId << ") - fallback mode" << std::endl;
    }

    void OpenALAudioEngine::pauseSound(uint32_t sourceId) {
        std::cout << "OpenAL Audio Engine: pauseSound(" << sourceId << ") - fallback mode" << std::endl;
    }

    void OpenALAudioEngine::stopSound(uint32_t sourceId) {
        std::cout << "OpenAL Audio Engine: stopSound(" << sourceId << ") - fallback mode" << std::endl;
    }

    void OpenALAudioEngine::stopAllSounds() {
        std::cout << "OpenAL Audio Engine: stopAllSounds() - fallback mode" << std::endl;
    }

    void OpenALAudioEngine::playStreamingSound(uint32_t sourceId, bool loop) {
        std::cout << "OpenAL Audio Engine: playStreamingSound(" << sourceId << ", " << loop << ") - fallback mode" << std::endl;
    }

    void OpenALAudioEngine::pauseStreamingSound(uint32_t sourceId) {
        std::cout << "OpenAL Audio Engine: pauseStreamingSound(" << sourceId << ") - fallback mode" << std::endl;
    }

    void OpenALAudioEngine::stopStreamingSound(uint32_t sourceId) {
        std::cout << "OpenAL Audio Engine: stopStreamingSound(" << sourceId << ") - fallback mode" << std::endl;
    }

    void OpenALAudioEngine::setSourcePosition(uint32_t sourceId, const glm::vec3& position) {
        // Silent fallback - position changes are common and don't need logging
    }

    void OpenALAudioEngine::setSourceVelocity(uint32_t sourceId, const glm::vec3& velocity) {
        // Silent fallback
    }

    void OpenALAudioEngine::setSourceDirection(uint32_t sourceId, const glm::vec3& direction) {
        // Silent fallback
    }

    glm::vec3 OpenALAudioEngine::getSourcePosition(uint32_t sourceId) const {
        return glm::vec3(0.0f);
    }

    void OpenALAudioEngine::setSourceGain(uint32_t sourceId, float gain) {
        // Silent fallback
    }

    void OpenALAudioEngine::setSourcePitch(uint32_t sourceId, float pitch) {
        // Silent fallback
    }

    void OpenALAudioEngine::setSourceLooping(uint32_t sourceId, bool looping) {
        // Silent fallback
    }

    void OpenALAudioEngine::setSourceRolloffFactor(uint32_t sourceId, float rolloff) {
        // Silent fallback
    }

    void OpenALAudioEngine::setSourceMaxDistance(uint32_t sourceId, float maxDistance) {
        // Silent fallback
    }

    void OpenALAudioEngine::setSourceReferenceDistance(uint32_t sourceId, float refDistance) {
        // Silent fallback
    }

    void OpenALAudioEngine::setListenerPosition(const glm::vec3& position) {
        if (m_listener) {
            m_listener->position = position;
        }
    }

    void OpenALAudioEngine::setListenerVelocity(const glm::vec3& velocity) {
        if (m_listener) {
            m_listener->velocity = velocity;
        }
    }

    void OpenALAudioEngine::setListenerOrientation(const glm::vec3& at, const glm::vec3& up) {
        if (m_listener) {
            m_listener->orientation[0] = at;
            m_listener->orientation[1] = up;
        }
    }

    void OpenALAudioEngine::setListenerGain(float gain) {
        if (m_listener) {
            m_listener->gain = gain;
        }
    }

    glm::vec3 OpenALAudioEngine::getListenerPosition() const {
        return m_listener ? m_listener->position : glm::vec3(0.0f);
    }

    bool OpenALAudioEngine::isPlaying(uint32_t sourceId) const {
        return false;
    }

    bool OpenALAudioEngine::isPaused(uint32_t sourceId) const {
        return false;
    }

    float OpenALAudioEngine::getSourceGain(uint32_t sourceId) const {
        return 1.0f;
    }

    void OpenALAudioEngine::update() {
        // Silent fallback - no update needed
    }

    size_t OpenALAudioEngine::getActiveSources() const {
        return 0;
    }

    void OpenALAudioEngine::setGlobalVolume(float volume) {
        m_globalVolume = std::max(0.0f, std::min(1.0f, volume));
    }

    float OpenALAudioEngine::getGlobalVolume() const {
        return m_globalVolume;
    }

    std::string OpenALAudioEngine::getLastError() const {
        return m_lastError;
    }

    void OpenALAudioEngine::clearErrors() {
        m_lastError.clear();
    }

    // Helper methods - keep the mathematical functions working
    float OpenALAudioEngine::calculateDistanceAttenuation(const glm::vec3& sourcePos, const glm::vec3& listenerPos, 
                                                         float referenceDistance, float maxDistance, float rolloffFactor) {
        float distance = glm::length(sourcePos - listenerPos);
        
        if (distance <= referenceDistance) {
            return 1.0f;
        }
        
        if (distance >= maxDistance) {
            return 0.0f;
        }
        
        return referenceDistance / (referenceDistance + rolloffFactor * (distance - referenceDistance));
    }

    glm::vec3 OpenALAudioEngine::calculateDopplerEffect(const glm::vec3& sourceVel, const glm::vec3& listenerVel,
                                                       const glm::vec3& sourcePos, const glm::vec3& listenerPos) {
        return glm::vec3(0.0f); // No Doppler effect in fallback mode
    }

    void OpenALAudioEngine::logALError(int error, const std::string& operation) const {
        // Fallback error logging
        if (error != 0) {
            std::cerr << "Audio Error (" << error << ") in operation: " << operation << " (OpenAL not available)" << std::endl;
        }
    }

    void OpenALAudioEngine::logALCError(void* device, const std::string& operation) const {
        // Fallback context error logging
        std::cerr << "Audio Context Error in operation: " << operation << " (OpenAL not available)" << std::endl;
    }

    void OpenALAudioEngine::printAudioDeviceInfo() const {
        std::cout << "OpenAL Device Info: Fallback mode (OpenAL not available)" << std::endl;
    }

    void OpenALAudioEngine::setDopplerFactor(float factor) {
        // No-op in fallback mode (OpenAL unavailable).
        (void)factor;
    }

    void OpenALAudioEngine::setSpeedOfSound(float speed) {
        // No-op in fallback mode (OpenAL unavailable).
        (void)speed;
    }

    size_t OpenALAudioEngine::getActiveSourceCount() const {
        return 0;
    }

    std::vector<std::string> OpenALAudioEngine::getLoadedSounds() const {
        return {};
    }

    size_t OpenALAudioEngine::getLoadedBufferCount() const {
        return 0;
    }

#endif // OPENAL_AVAILABLE

    // AudioSystem singleton implementation
    AudioSystem& AudioSystem::getInstance() {
        static AudioSystem instance;
        return instance;
    }

} // namespace IKore
