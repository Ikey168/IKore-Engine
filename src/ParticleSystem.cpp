#include "ParticleSystem.h"
#include "Logger.h"
#include <stb_image.h>
#include <algorithm>
#include <random>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

namespace IKore {

namespace {
    // Random number generation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
}

// ===== ParticleSystem Implementation =====

ParticleSystem::ParticleSystem()
    : m_particleVAO(0), m_particleVBO(0), m_particleSSBO(0), m_emitterUBO(0),
      m_particleTexture(0), m_updateComputeShader(0), m_emitComputeShader(0),
      m_maxParticles(0), m_activeParticles(0), m_emissionTimer(0.0f),
      m_initialized(false), m_enabled(true), m_playing(false),
      m_lastUpdateTime(0.0f), m_computeShadersSupported(false), m_maxWorkGroupSize(0) {
}

ParticleSystem::~ParticleSystem() {
    cleanup();
}

bool ParticleSystem::initialize(int maxParticles) {
    if (m_initialized) {
        cleanup();
    }
    
    m_maxParticles = maxParticles;
    m_particles.resize(maxParticles);
    
    // Check compute shader support
    checkComputeShaderSupport();
    
    // Initialize buffers
    if (!initializeBuffers()) {
        LOG_ERROR("Failed to initialize particle system buffers");
        return false;
    }
    
    // Load shaders
    if (!loadShaders()) {
        LOG_ERROR("Failed to load particle system shaders");
        return false;
    }
    
    // Create compute shaders if supported
    if (m_computeShadersSupported) {
        if (!createComputeShaders()) {
            LOG_WARNING("Failed to create compute shaders, falling back to CPU updates");
            m_computeShadersSupported = false;
        }
    }
    
    // Setup vertex attributes
    setupVertexAttributes();
    
    // Load default texture (white square for testing)
    glGenTextures(1, &m_particleTexture);
    glBindTexture(GL_TEXTURE_2D, m_particleTexture);
    
    // Create a simple white texture
    unsigned char whitePixel[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Initialize with default fire preset
    loadPreset(ParticleEffectType::FIRE);
    
    m_initialized = true;
    LOG_INFO("Particle system initialized with " + std::to_string(maxParticles) + " particles");
    LOG_INFO("Compute shaders " + std::string(m_computeShadersSupported ? "enabled" : "disabled"));
    
    return true;
}

void ParticleSystem::cleanup() {
    if (m_particleVAO) glDeleteVertexArrays(1, &m_particleVAO);
    if (m_particleVBO) glDeleteBuffers(1, &m_particleVBO);
    if (m_particleSSBO) glDeleteBuffers(1, &m_particleSSBO);
    if (m_emitterUBO) glDeleteBuffers(1, &m_emitterUBO);
    if (m_particleTexture) glDeleteTextures(1, &m_particleTexture);
    if (m_updateComputeShader) glDeleteProgram(m_updateComputeShader);
    if (m_emitComputeShader) glDeleteProgram(m_emitComputeShader);
    
    m_particleVAO = m_particleVBO = m_particleSSBO = m_emitterUBO = 0;
    m_particleTexture = m_updateComputeShader = m_emitComputeShader = 0;
    m_initialized = false;
}

bool ParticleSystem::initializeBuffers() {
    // Generate VAO and VBO for particle rendering
    glGenVertexArrays(1, &m_particleVAO);
    glGenBuffers(1, &m_particleVBO);
    
    // Initialize particle data with empty particles
    for (auto& particle : m_particles) {
        particle.life = 0.0f; // Dead particle
        particle.position = glm::vec3(0.0f);
        particle.velocity = glm::vec3(0.0f);
        particle.acceleration = glm::vec3(0.0f);
        particle.color = glm::vec4(1.0f);
        particle.size = 1.0f;
        particle.startLife = 0.0f;
        particle.rotation = glm::vec2(0.0f);
        particle.mass = 1.0f;
        particle.padding = 0.0f;
    }
    
    // Create SSBO for particle data (for compute shaders)
    if (m_computeShadersSupported) {
        glGenBuffers(1, &m_particleSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_particles.size() * sizeof(Particle), 
                     m_particles.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_particleSSBO);
    }
    
    // Create UBO for emitter configuration
    glGenBuffers(1, &m_emitterUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_emitterUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ParticleEmitterConfig), &m_config, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_emitterUBO);
    
    return true;
}

bool ParticleSystem::loadShaders() {
    std::string shaderError;
    
    // Load rendering shaders
    m_renderShader = Shader::loadFromFilesCached(
        "src/shaders/particle.vert", 
        "src/shaders/particle.frag", 
        shaderError
    );
    
    if (!m_renderShader) {
        LOG_ERROR("Failed to load particle rendering shaders: " + shaderError);
        return false;
    }
    
    return true;
}

bool ParticleSystem::createComputeShaders() {
    // For now, return false to use CPU fallback
    // Compute shaders will be implemented after basic system works
    return false;
}

void ParticleSystem::setupVertexAttributes() {
    glBindVertexArray(m_particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_particleVBO);
    
    // Allocate buffer for particle rendering data
    // Each particle needs: position(3), size(1), color(4), rotation(1) = 9 floats
    glBufferData(GL_ARRAY_BUFFER, m_maxParticles * 9 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    
    // Position (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    
    // Size (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    
    // Color (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(4 * sizeof(float)));
    
    // Rotation (location = 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(8 * sizeof(float)));
    
    glBindVertexArray(0);
}

void ParticleSystem::checkComputeShaderSupport() {
    // Check if compute shaders are supported
    GLint computeShaderSupported = 0;
    glGetIntegerv(GL_COMPUTE_SHADER, &computeShaderSupported);
    
    if (computeShaderSupported) {
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_SIZE, &m_maxWorkGroupSize);
        m_computeShadersSupported = true;
        LOG_INFO("Compute shaders supported, max work group size: " + std::to_string(m_maxWorkGroupSize));
    } else {
        m_computeShadersSupported = false;
        LOG_INFO("Compute shaders not supported, using CPU fallback");
    }
}

void ParticleSystem::setEmitterConfig(const ParticleEmitterConfig& config) {
    m_config = config;
    updateEmitterBuffer();
}

void ParticleSystem::updateEmitterBuffer() {
    if (m_emitterUBO) {
        glBindBuffer(GL_UNIFORM_BUFFER, m_emitterUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ParticleEmitterConfig), &m_config);
    }
}

void ParticleSystem::loadPreset(ParticleEffectType type) {
    switch (type) {
        case ParticleEffectType::FIRE:
            m_config = createFirePreset();
            break;
        case ParticleEffectType::SMOKE:
            m_config = createSmokePreset();
            break;
        case ParticleEffectType::EXPLOSION:
            m_config = createExplosionPreset();
            break;
        case ParticleEffectType::SPARKS:
            m_config = createSparksPreset();
            break;
        case ParticleEffectType::MAGIC:
            m_config = createMagicPreset();
            break;
        case ParticleEffectType::RAIN:
            m_config = createRainPreset();
            break;
        case ParticleEffectType::SNOW:
            m_config = createSnowPreset();
            break;
        default:
            // Keep current config for CUSTOM
            break;
    }
    updateEmitterBuffer();
}

void ParticleSystem::setTexture(const std::string& texturePath) {
    int width, height, channels;
    unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &channels, 4);
    
    if (!data) {
        LOG_WARNING("Failed to load particle texture: " + texturePath);
        return;
    }
    
    if (m_particleTexture) {
        glDeleteTextures(1, &m_particleTexture);
    }
    
    glGenTextures(1, &m_particleTexture);
    glBindTexture(GL_TEXTURE_2D, m_particleTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    stbi_image_free(data);
    LOG_INFO("Loaded particle texture: " + texturePath);
}

void ParticleSystem::setTexture(GLuint textureID) {
    m_particleTexture = textureID;
}

void ParticleSystem::play() {
    m_playing = true;
}

void ParticleSystem::pause() {
    m_playing = false;
}

void ParticleSystem::stop() {
    m_playing = false;
    reset();
}

void ParticleSystem::reset() {
    m_activeParticles = 0;
    m_emissionTimer = 0.0f;
    
    // Kill all particles
    for (auto& particle : m_particles) {
        particle.life = 0.0f;
    }
}

void ParticleSystem::update(float deltaTime) {
    if (!m_initialized || !m_enabled) return;
    
    m_lastUpdateTime = deltaTime;
    
    if (m_playing) {
        // Emit new particles
        emitParticles(deltaTime);
    }
    
    // Update particles
    if (m_computeShadersSupported) {
        updateGPU(deltaTime);
    } else {
        updateCPU(deltaTime);
    }
}

void ParticleSystem::updateCPU(float deltaTime) {
    m_activeParticles = 0;
    
    for (auto& particle : m_particles) {
        if (particle.life <= 0.0f) continue;
        
        // Update life
        particle.life -= deltaTime / particle.startLife;
        
        if (particle.life <= 0.0f) {
            particle.life = 0.0f;
            continue;
        }
        
        // Update physics
        particle.velocity += particle.acceleration * deltaTime;
        particle.position += particle.velocity * deltaTime;
        
        // Update rotation
        particle.rotation.x += particle.rotation.y * deltaTime;
        
        m_activeParticles++;
    }
}

void ParticleSystem::updateGPU(float deltaTime) {
    // GPU update using compute shaders
    // This is a placeholder for future implementation
    updateCPU(deltaTime); // Fallback to CPU for now
}

void ParticleSystem::emitParticles(float deltaTime) {
    if (!m_config.looping && m_emissionTimer > m_config.startLife) {
        return; // Stop emitting for non-looping effects
    }
    
    m_emissionTimer += deltaTime;
    float particlesToEmit = m_config.emissionRate * deltaTime;
    
    // Handle fractional particles
    static float particleRemainder = 0.0f;
    particlesToEmit += particleRemainder;
    int wholeParticles = static_cast<int>(particlesToEmit);
    particleRemainder = particlesToEmit - wholeParticles;
    
    emit(wholeParticles);
}

void ParticleSystem::emit(int count) {
    if (!m_initialized || count <= 0) return;
    
    int emitted = 0;
    for (int i = 0; i < m_maxParticles && emitted < count; ++i) {
        if (m_particles[i].life <= 0.0f) {
            initializeParticle(m_particles[i]);
            emitted++;
        }
    }
}

void ParticleSystem::burst(int count) {
    emit(count);
}

void ParticleSystem::initializeParticle(Particle& particle) {
    particle.position = randomVec3(m_config.position, m_config.positionVariance);
    particle.velocity = randomVec3(m_config.velocity, m_config.velocityVariance);
    particle.acceleration = randomVec3(m_config.acceleration, m_config.accelerationVariance);
    particle.color = randomVec4(m_config.startColor, m_config.colorVariance);
    particle.size = randomFloat(
        m_config.startSize - m_config.sizeVariance,
        m_config.startSize + m_config.sizeVariance
    );
    particle.startLife = randomFloat(
        m_config.startLife - m_config.lifeVariance,
        m_config.startLife + m_config.lifeVariance
    );
    particle.life = 1.0f; // Full life (0-1 range)
    particle.rotation = glm::vec2(
        randomFloat(0.0f, 360.0f),  // Initial rotation
        randomFloat(-180.0f, 180.0f) // Angular velocity
    );
    particle.mass = randomFloat(
        m_config.mass - m_config.massVariance,
        m_config.mass + m_config.massVariance
    );
}

float ParticleSystem::randomFloat(float min, float max) {
    return min + dis(gen) * (max - min);
}

glm::vec3 ParticleSystem::randomVec3(const glm::vec3& base, const glm::vec3& variance) {
    return base + glm::vec3(
        randomFloat(-variance.x, variance.x),
        randomFloat(-variance.y, variance.y),
        randomFloat(-variance.z, variance.z)
    );
}

glm::vec4 ParticleSystem::randomVec4(const glm::vec4& base, const glm::vec4& variance) {
    return base + glm::vec4(
        randomFloat(-variance.x, variance.x),
        randomFloat(-variance.y, variance.y),
        randomFloat(-variance.z, variance.z),
        randomFloat(-variance.w, variance.w)
    );
}

void ParticleSystem::render(const glm::mat4& view, const glm::mat4& projection) {
    if (!m_initialized || !m_enabled || !m_renderShader || m_activeParticles == 0) return;
    
    // Enable blending for particles
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Don't write to depth buffer
    
    // Use particle shader
    m_renderShader->use();
    m_renderShader->setMat4("view", glm::value_ptr(view));
    m_renderShader->setMat4("projection", glm::value_ptr(projection));
    
    // Bind particle texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_particleTexture);
    m_renderShader->setInt("particleTexture", 0);
    
    // Update vertex buffer with current particle data
    std::vector<float> renderData;
    renderData.reserve(m_activeParticles * 9);
    
    for (const auto& particle : m_particles) {
        if (particle.life <= 0.0f) continue;
        
        // Interpolate color and size based on life
        float lifeRatio = particle.life;
        glm::vec4 currentColor = glm::mix(m_config.endColor, m_config.startColor, lifeRatio);
        float currentSize = glm::mix(m_config.endSize, m_config.startSize, lifeRatio);
        
        // Add particle data: position(3), size(1), color(4), rotation(1)
        renderData.push_back(particle.position.x);
        renderData.push_back(particle.position.y);
        renderData.push_back(particle.position.z);
        renderData.push_back(currentSize);
        renderData.push_back(currentColor.r);
        renderData.push_back(currentColor.g);
        renderData.push_back(currentColor.b);
        renderData.push_back(currentColor.a);
        renderData.push_back(particle.rotation.x);
    }
    
    // Upload data to GPU
    glBindVertexArray(m_particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_particleVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, renderData.size() * sizeof(float), renderData.data());
    
    // Render particles as points
    glDrawArrays(GL_POINTS, 0, m_activeParticles);
    
    glBindVertexArray(0);
    
    // Restore render state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// Setter methods
void ParticleSystem::setPosition(const glm::vec3& position) {
    m_config.position = position;
    updateEmitterBuffer();
}

void ParticleSystem::setEmissionRate(float rate) {
    m_config.emissionRate = rate;
    updateEmitterBuffer();
}

void ParticleSystem::setMaxParticles(int max) {
    if (max != m_maxParticles) {
        m_maxParticles = max;
        m_particles.resize(max);
        // Re-initialize buffers
        initializeBuffers();
        setupVertexAttributes();
    }
}

void ParticleSystem::setGravity(const glm::vec3& gravity) {
    m_config.acceleration = gravity;
    updateEmitterBuffer();
}

void ParticleSystem::setColors(const glm::vec4& start, const glm::vec4& end) {
    m_config.startColor = start;
    m_config.endColor = end;
    updateEmitterBuffer();
}

void ParticleSystem::setSizes(float start, float end) {
    m_config.startSize = start;
    m_config.endSize = end;
    updateEmitterBuffer();
}

void ParticleSystem::setLifetime(float life) {
    m_config.startLife = life;
    updateEmitterBuffer();
}

// Preset configurations
ParticleEmitterConfig ParticleSystem::createFirePreset() {
    ParticleEmitterConfig config;
    config.position = glm::vec3(0.0f, 0.0f, 0.0f);
    config.positionVariance = glm::vec3(0.2f, 0.1f, 0.2f);
    config.velocity = glm::vec3(0.0f, 2.0f, 0.0f);
    config.velocityVariance = glm::vec3(0.5f, 1.0f, 0.5f);
    config.acceleration = glm::vec3(0.0f, 1.0f, 0.0f); // Rising heat
    config.startColor = glm::vec4(1.0f, 0.2f, 0.0f, 1.0f); // Orange
    config.endColor = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);   // Yellow fade
    config.startSize = 0.5f;
    config.endSize = 1.5f;
    config.startLife = 2.0f;
    config.emissionRate = 50.0f;
    config.maxParticles = 200.0f;
    return config;
}

ParticleEmitterConfig ParticleSystem::createSmokePreset() {
    ParticleEmitterConfig config;
    config.position = glm::vec3(0.0f, 0.0f, 0.0f);
    config.positionVariance = glm::vec3(0.3f, 0.1f, 0.3f);
    config.velocity = glm::vec3(0.0f, 1.5f, 0.0f);
    config.velocityVariance = glm::vec3(0.8f, 0.5f, 0.8f);
    config.acceleration = glm::vec3(0.0f, 0.5f, 0.0f);
    config.startColor = glm::vec4(0.8f, 0.8f, 0.8f, 0.8f); // Light gray
    config.endColor = glm::vec4(0.4f, 0.4f, 0.4f, 0.0f);   // Dark gray fade
    config.startSize = 0.8f;
    config.endSize = 3.0f;
    config.startLife = 4.0f;
    config.emissionRate = 30.0f;
    config.maxParticles = 150.0f;
    return config;
}

ParticleEmitterConfig ParticleSystem::createExplosionPreset() {
    ParticleEmitterConfig config;
    config.position = glm::vec3(0.0f, 0.0f, 0.0f);
    config.positionVariance = glm::vec3(0.1f, 0.1f, 0.1f);
    config.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    config.velocityVariance = glm::vec3(5.0f, 5.0f, 5.0f); // Explosive spread
    config.acceleration = glm::vec3(0.0f, -9.81f, 0.0f);   // Gravity
    config.startColor = glm::vec4(1.0f, 0.8f, 0.0f, 1.0f); // Bright yellow
    config.endColor = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);   // Red fade
    config.startSize = 0.3f;
    config.endSize = 0.1f;
    config.startLife = 3.0f;
    config.emissionRate = 0.0f; // Burst only
    config.maxParticles = 500.0f;
    config.looping = false;
    return config;
}

ParticleEmitterConfig ParticleSystem::createSparksPreset() {
    ParticleEmitterConfig config;
    config.position = glm::vec3(0.0f, 0.0f, 0.0f);
    config.positionVariance = glm::vec3(0.1f, 0.1f, 0.1f);
    config.velocity = glm::vec3(0.0f, 2.0f, 0.0f);
    config.velocityVariance = glm::vec3(3.0f, 1.0f, 3.0f);
    config.acceleration = glm::vec3(0.0f, -15.0f, 0.0f); // Strong gravity
    config.startColor = glm::vec4(1.0f, 1.0f, 0.8f, 1.0f); // Bright white-yellow
    config.endColor = glm::vec4(1.0f, 0.5f, 0.0f, 0.0f);   // Orange fade
    config.startSize = 0.2f;
    config.endSize = 0.05f;
    config.startLife = 1.5f;
    config.emissionRate = 80.0f;
    config.maxParticles = 100.0f;
    return config;
}

ParticleEmitterConfig ParticleSystem::createMagicPreset() {
    ParticleEmitterConfig config;
    config.position = glm::vec3(0.0f, 0.0f, 0.0f);
    config.positionVariance = glm::vec3(0.5f, 0.5f, 0.5f);
    config.velocity = glm::vec3(0.0f, 0.5f, 0.0f);
    config.velocityVariance = glm::vec3(1.0f, 1.0f, 1.0f);
    config.acceleration = glm::vec3(0.0f, 0.0f, 0.0f); // Floating
    config.startColor = glm::vec4(0.5f, 0.0f, 1.0f, 1.0f); // Purple
    config.endColor = glm::vec4(0.0f, 1.0f, 1.0f, 0.0f);   // Cyan fade
    config.startSize = 0.3f;
    config.endSize = 0.6f;
    config.startLife = 3.0f;
    config.emissionRate = 25.0f;
    config.maxParticles = 100.0f;
    return config;
}

ParticleEmitterConfig ParticleSystem::createRainPreset() {
    ParticleEmitterConfig config;
    config.position = glm::vec3(0.0f, 10.0f, 0.0f);
    config.positionVariance = glm::vec3(5.0f, 0.0f, 5.0f);
    config.velocity = glm::vec3(0.0f, -10.0f, 0.0f);
    config.velocityVariance = glm::vec3(0.2f, 2.0f, 0.2f);
    config.acceleration = glm::vec3(0.0f, -5.0f, 0.0f);
    config.startColor = glm::vec4(0.7f, 0.8f, 1.0f, 0.8f); // Light blue
    config.endColor = glm::vec4(0.5f, 0.6f, 0.8f, 0.3f);
    config.startSize = 0.1f;
    config.endSize = 0.05f;
    config.startLife = 2.0f;
    config.emissionRate = 200.0f;
    config.maxParticles = 500.0f;
    return config;
}

ParticleEmitterConfig ParticleSystem::createSnowPreset() {
    ParticleEmitterConfig config;
    config.position = glm::vec3(0.0f, 8.0f, 0.0f);
    config.positionVariance = glm::vec3(10.0f, 0.0f, 10.0f);
    config.velocity = glm::vec3(0.0f, -1.0f, 0.0f);
    config.velocityVariance = glm::vec3(0.5f, 0.5f, 0.5f);
    config.acceleration = glm::vec3(0.0f, -0.5f, 0.0f); // Gentle fall
    config.startColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // White
    config.endColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f);
    config.startSize = 0.3f;
    config.endSize = 0.2f;
    config.startLife = 8.0f;
    config.emissionRate = 50.0f;
    config.maxParticles = 300.0f;
    return config;
}

// ===== ParticleSystemManager Implementation =====

ParticleSystemManager::ParticleSystemManager() : m_globalEnabled(true) {
}

ParticleSystemManager::~ParticleSystemManager() {
    destroyAllSystems();
}

ParticleSystem* ParticleSystemManager::createSystem(int maxParticles) {
    auto system = std::make_unique<ParticleSystem>();
    if (system->initialize(maxParticles)) {
        ParticleSystem* ptr = system.get();
        m_systems.push_back(std::move(system));
        return ptr;
    }
    return nullptr;
}

void ParticleSystemManager::destroySystem(ParticleSystem* system) {
    auto it = std::find_if(m_systems.begin(), m_systems.end(),
        [system](const std::unique_ptr<ParticleSystem>& ptr) {
            return ptr.get() == system;
        });
    
    if (it != m_systems.end()) {
        m_systems.erase(it);
    }
}

void ParticleSystemManager::destroyAllSystems() {
    m_systems.clear();
}

void ParticleSystemManager::updateAll(float deltaTime) {
    if (!m_globalEnabled) return;
    
    for (auto& system : m_systems) {
        system->update(deltaTime);
    }
}

void ParticleSystemManager::renderAll(const glm::mat4& view, const glm::mat4& projection) {
    if (!m_globalEnabled) return;
    
    for (auto& system : m_systems) {
        system->render(view, projection);
    }
}

void ParticleSystemManager::playAll() {
    for (auto& system : m_systems) {
        system->play();
    }
}

void ParticleSystemManager::pauseAll() {
    for (auto& system : m_systems) {
        system->pause();
    }
}

void ParticleSystemManager::stopAll() {
    for (auto& system : m_systems) {
        system->stop();
    }
}

int ParticleSystemManager::getTotalActiveParticles() const {
    int total = 0;
    for (const auto& system : m_systems) {
        total += system->getActiveParticles();
    }
    return total;
}

ParticleSystem* ParticleSystemManager::createFireEffect(const glm::vec3& position) {
    ParticleSystem* system = createSystem(200);
    if (system) {
        system->loadPreset(ParticleEffectType::FIRE);
        system->setPosition(position);
        system->play();
    }
    return system;
}

ParticleSystem* ParticleSystemManager::createSmokeEffect(const glm::vec3& position) {
    ParticleSystem* system = createSystem(150);
    if (system) {
        system->loadPreset(ParticleEffectType::SMOKE);
        system->setPosition(position);
        system->play();
    }
    return system;
}

ParticleSystem* ParticleSystemManager::createExplosionEffect(const glm::vec3& position) {
    ParticleSystem* system = createSystem(500);
    if (system) {
        system->loadPreset(ParticleEffectType::EXPLOSION);
        system->setPosition(position);
        system->burst(500); // Immediate burst
    }
    return system;
}

ParticleSystem* ParticleSystemManager::createSparksEffect(const glm::vec3& position) {
    ParticleSystem* system = createSystem(100);
    if (system) {
        system->loadPreset(ParticleEffectType::SPARKS);
        system->setPosition(position);
        system->play();
    }
    return system;
}

} // namespace IKore
