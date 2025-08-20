#pragma once

#include "../Component.h"
#include "../../ParticleSystem.h"
#include <memory>
#include <string>

namespace IKore {

/**
 * @brief Component for attaching particle effects to entities
 * 
 * This component allows entities to emit particle effects such as fire,
 * smoke, sparks, and explosions. It wraps the ParticleSystem to make it
 * easily attachable to any entity.
 */
class ParticleComponent : public Component {
public:
    /**
     * @brief Default constructor
     */
    ParticleComponent();
    
    /**
     * @brief Constructor with predefined effect type
     * @param type The type of particle effect to create
     */
    explicit ParticleComponent(ParticleEffectType type);
    
    /**
     * @brief Destructor
     */
    virtual ~ParticleComponent();
    
    /**
     * @brief Initialize the particle component
     * @param maxParticles Maximum number of particles
     * @return True if initialization succeeded
     */
    bool initialize(int maxParticles = 1000);
    
    /**
     * @brief Called when the component is attached to an entity
     */
    void onAttach() override;
    
    /**
     * @brief Called when the component is detached from an entity
     */
    void onDetach() override;
    
    /**
     * @brief Update the particle system
     * @param deltaTime Time elapsed since last update
     */
    void update(float deltaTime);
    
    /**
     * @brief Render the particle system
     * @param view The view matrix
     * @param projection The projection matrix
     */
    void render(const glm::mat4& view, const glm::mat4& projection);
    
    /**
     * @brief Start emitting particles
     */
    void play();
    
    /**
     * @brief Pause particle emission
     */
    void pause();
    
    /**
     * @brief Stop particle emission and clear existing particles
     */
    void stop();
    
    /**
     * @brief Reset the particle system
     */
    void reset();
    
    /**
     * @brief Emit a burst of particles
     * @param count Number of particles to emit
     */
    void burst(int count);
    
    /**
     * @brief Set the particle effect type
     * @param type The predefined effect type
     */
    void setEffectType(ParticleEffectType type);
    
    /**
     * @brief Set the texture for particles
     * @param texturePath Path to the texture file
     */
    void setTexture(const std::string& texturePath);
    
    /**
     * @brief Set the emitter configuration
     * @param config The emitter configuration
     */
    void setEmitterConfig(const ParticleEmitterConfig& config);
    
    /**
     * @brief Get the current emitter configuration
     * @return Reference to the current configuration
     */
    const ParticleEmitterConfig& getEmitterConfig() const;
    
    /**
     * @brief Set the emission rate
     * @param rate Particles per second
     */
    void setEmissionRate(float rate);
    
    /**
     * @brief Set the maximum number of particles
     * @param max Maximum number of particles
     */
    void setMaxParticles(int max);
    
    /**
     * @brief Set the gravity affecting particles
     * @param gravity The gravity vector
     */
    void setGravity(const glm::vec3& gravity);
    
    /**
     * @brief Set the start and end colors for particles
     * @param start Start color (RGBA)
     * @param end End color (RGBA)
     */
    void setColors(const glm::vec4& start, const glm::vec4& end);
    
    /**
     * @brief Set the start and end sizes for particles
     * @param start Start size
     * @param end End size
     */
    void setSizes(float start, float end);
    
    /**
     * @brief Set the lifetime for particles
     * @param life Lifetime in seconds
     */
    void setLifetime(float life);
    
    /**
     * @brief Check if the particle system is playing
     * @return True if the system is emitting particles
     */
    bool isPlaying() const;
    
    /**
     * @brief Get the number of active particles
     * @return Number of active particles
     */
    int getActiveParticles() const;

private:
    std::unique_ptr<ParticleSystem> m_particleSystem;
    ParticleEffectType m_effectType;
    bool m_followEntity;
    bool m_initialized;
    
    /**
     * @brief Update the particle system position to match the entity position
     */
    void updatePosition();
};

} // namespace IKore
