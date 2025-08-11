#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>
#include <functional>
#include "Shader.h"
#include "Texture.h"

namespace IKore {

/**
 * @brief Particle data structure for GPU computation
 */
struct Particle {
    glm::vec3 position;     // Current position
    float life;             // Current life (0.0 to 1.0)
    glm::vec3 velocity;     // Current velocity
    float size;             // Particle size
    glm::vec4 color;        // Particle color (RGBA)
    glm::vec3 acceleration; // Acceleration (gravity, forces)
    float startLife;        // Initial life value
    glm::vec2 rotation;     // Rotation (angle, angular velocity)
    float mass;             // Particle mass
    float padding;          // Padding for alignment
};

/**
 * @brief Particle emitter configuration
 */
struct ParticleEmitterConfig {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 positionVariance{0.1f, 0.1f, 0.1f};
    
    glm::vec3 velocity{0.0f, 1.0f, 0.0f};
    glm::vec3 velocityVariance{0.5f, 0.5f, 0.5f};
    
    glm::vec3 acceleration{0.0f, -9.81f, 0.0f}; // Gravity
    glm::vec3 accelerationVariance{0.1f, 0.1f, 0.1f};
    
    glm::vec4 startColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 endColor{1.0f, 1.0f, 1.0f, 0.0f};
    glm::vec4 colorVariance{0.1f, 0.1f, 0.1f, 0.0f};
    
    float startSize = 1.0f;
    float endSize = 0.5f;
    float sizeVariance = 0.2f;
    
    float startLife = 3.0f;
    float lifeVariance = 1.0f;
    
    float emissionRate = 100.0f;    // particles per second
    float maxParticles = 1000.0f;
    
    float mass = 1.0f;
    float massVariance = 0.1f;
    
    bool looping = true;
    bool worldSpace = true;
};

/**
 * @brief Predefined particle effect types
 */
enum class ParticleEffectType {
    FIRE,
    SMOKE,
    EXPLOSION,
    SPARKS,
    MAGIC,
    RAIN,
    SNOW,
    CUSTOM
};

/**
 * @brief GPU-based particle system with compute shaders
 */
class ParticleSystem {
private:
    // OpenGL objects
    GLuint m_particleVAO, m_particleVBO;
    GLuint m_particleSSBO;              // Shader Storage Buffer for particles
    GLuint m_emitterUBO;                // Uniform Buffer for emitter config
    GLuint m_particleTexture;           // Particle sprite texture
    
    // Shaders
    std::shared_ptr<Shader> m_renderShader;
    GLuint m_updateComputeShader;       // Compute shader for particle updates
    GLuint m_emitComputeShader;         // Compute shader for particle emission
    
    // Particle data
    std::vector<Particle> m_particles;
    ParticleEmitterConfig m_config;
    
    // State
    int m_maxParticles;
    int m_activeParticles;
    float m_emissionTimer;
    bool m_initialized;
    bool m_enabled;
    bool m_playing;
    
    // Performance tracking
    float m_lastUpdateTime;
    
    // GPU capabilities
    bool m_computeShadersSupported;
    int m_maxWorkGroupSize;
    
public:
    ParticleSystem();
    ~ParticleSystem();
    
    // Delete copy operations for resource management
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;
    
    // Initialization and cleanup
    bool initialize(int maxParticles = 1000);
    void cleanup();
    
    // Configuration
    void setEmitterConfig(const ParticleEmitterConfig& config);
    const ParticleEmitterConfig& getEmitterConfig() const { return m_config; }
    
    void loadPreset(ParticleEffectType type);
    void setTexture(const std::string& texturePath);
    void setTexture(GLuint textureID);
    
    // Control
    void play();
    void pause();
    void stop();
    void reset();
    
    bool isPlaying() const { return m_playing; }
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    
    // Update and rendering
    void update(float deltaTime);
    void render(const glm::mat4& view, const glm::mat4& projection);
    
    // Particle management
    void emit(int count = 1);
    void burst(int count);
    
    // Getters
    int getActiveParticles() const { return m_activeParticles; }
    int getMaxParticles() const { return m_maxParticles; }
    float getEmissionRate() const { return m_config.emissionRate; }
    
    // Setters for runtime adjustment
    void setPosition(const glm::vec3& position);
    void setEmissionRate(float rate);
    void setMaxParticles(int max);
    void setGravity(const glm::vec3& gravity);
    void setColors(const glm::vec4& start, const glm::vec4& end);
    void setSizes(float start, float end);
    void setLifetime(float life);
    
private:
    // Internal methods
    bool initializeBuffers();
    bool loadShaders();
    bool createComputeShaders();
    void setupVertexAttributes();
    
    void updateCPU(float deltaTime);      // Fallback CPU update
    void updateGPU(float deltaTime);      // GPU compute shader update
    
    void emitParticles(float deltaTime);
    void initializeParticle(Particle& particle);
    float randomFloat(float min, float max);
    glm::vec3 randomVec3(const glm::vec3& base, const glm::vec3& variance);
    glm::vec4 randomVec4(const glm::vec4& base, const glm::vec4& variance);
    
    void updateEmitterBuffer();
    void checkComputeShaderSupport();
    
    // Preset configurations
    ParticleEmitterConfig createFirePreset();
    ParticleEmitterConfig createSmokePreset();
    ParticleEmitterConfig createExplosionPreset();
    ParticleEmitterConfig createSparksPreset();
    ParticleEmitterConfig createMagicPreset();
    ParticleEmitterConfig createRainPreset();
    ParticleEmitterConfig createSnowPreset();
};

/**
 * @brief Particle system manager for multiple systems
 */
class ParticleSystemManager {
private:
    std::vector<std::unique_ptr<ParticleSystem>> m_systems;
    bool m_globalEnabled;
    
public:
    ParticleSystemManager();
    ~ParticleSystemManager();
    
    // System management
    ParticleSystem* createSystem(int maxParticles = 1000);
    void destroySystem(ParticleSystem* system);
    void destroyAllSystems();
    
    // Global control
    void updateAll(float deltaTime);
    void renderAll(const glm::mat4& view, const glm::mat4& projection);
    
    void playAll();
    void pauseAll();
    void stopAll();
    
    void setGlobalEnabled(bool enabled) { m_globalEnabled = enabled; }
    bool isGlobalEnabled() const { return m_globalEnabled; }
    
    // Getters
    size_t getSystemCount() const { return m_systems.size(); }
    int getTotalActiveParticles() const;
    
    // Quick effect creation
    ParticleSystem* createFireEffect(const glm::vec3& position);
    ParticleSystem* createSmokeEffect(const glm::vec3& position);
    ParticleSystem* createExplosionEffect(const glm::vec3& position);
    ParticleSystem* createSparksEffect(const glm::vec3& position);
};

} // namespace IKore
