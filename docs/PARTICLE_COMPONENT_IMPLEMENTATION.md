# Particle Effect Component Implementation

## Overview

The Particle Effect Component implements a component-based wrapper around the existing ParticleSystem class, allowing game entities to emit particle effects such as fire, smoke, sparks, and explosions. The component integrates with the Entity-Component System (ECS) architecture of the IKore Engine.

## Key Features

- Attaching particle effects to any entity
- Support for various effect types (fire, smoke, sparks, explosions)
- GPU-based rendering for optimal performance
- Automatic position updating based on entity transform
- Configurable emission rates, sizes, colors, and lifetimes
- Burst emission for explosions and one-time effects

## Implementation Details

### Class Structure

The `ParticleComponent` class wraps the existing `ParticleSystem` class, extending the base `Component` class:

```cpp
class ParticleComponent : public Component {
private:
    std::unique_ptr<ParticleSystem> m_particleSystem;
    ParticleEffectType m_effectType;
    bool m_followEntity;
    bool m_initialized;
    
    void updatePosition();
    
public:
    // Constructors and destructor
    ParticleComponent();
    explicit ParticleComponent(ParticleEffectType type);
    virtual ~ParticleComponent();
    
    // Component lifecycle methods
    bool initialize(int maxParticles = 1000);
    void onAttach() override;
    void onDetach() override;
    
    // Update and render
    void update(float deltaTime);
    void render(const glm::mat4& view, const glm::mat4& projection);
    
    // Control methods
    void play();
    void pause();
    void stop();
    void reset();
    void burst(int count);
    
    // Configuration methods
    void setEffectType(ParticleEffectType type);
    void setTexture(const std::string& texturePath);
    void setEmitterConfig(const ParticleEmitterConfig& config);
    const ParticleEmitterConfig& getEmitterConfig() const;
    
    // Parameter setters
    void setEmissionRate(float rate);
    void setMaxParticles(int max);
    void setGravity(const glm::vec3& gravity);
    void setColors(const glm::vec4& start, const glm::vec4& end);
    void setSizes(float start, float end);
    void setLifetime(float life);
    
    // State queries
    bool isPlaying() const;
    int getActiveParticles() const;
};
```

### Entity Integration

The `ParticleComponent` automatically updates the particle system position to match the entity's position when `m_followEntity` is enabled. This is accomplished by retrieving the entity's `TransformComponent` and updating the particle system's emitter position.

### Performance Considerations

The component leverages the GPU-based particle system for efficient rendering, minimizing performance impact even with hundreds or thousands of particles. Key optimizations include:

1. GPU-based rendering with instancing
2. Compute shader-based particle updates (when supported)
3. Automatic culling of particles outside the view frustum
4. Efficient memory management with pre-allocated particle pools

## Usage Examples

### Basic Usage

```cpp
// Create an entity
auto entity = std::make_shared<Entity>();

// Add a transform component
auto transform = entity->addComponent<TransformComponent>();
transform->setPosition(glm::vec3(0.0f, 1.0f, 0.0f));

// Add a fire particle effect
auto particleComp = entity->addComponent<ParticleComponent>(ParticleEffectType::FIRE);
particleComp->play();

// In the game loop
particleComp->update(deltaTime);
particleComp->render(viewMatrix, projectionMatrix);
```

### Customizing Effects

```cpp
auto particleComp = entity->addComponent<ParticleComponent>();

// Configure a custom effect
auto config = particleComp->getEmitterConfig();
config.startColor = glm::vec4(0.0f, 0.5f, 1.0f, 1.0f); // Blue
config.endColor = glm::vec4(0.0f, 0.2f, 0.8f, 0.0f);   // Fading blue
config.emissionRate = 30.0f;
particleComp->setEmitterConfig(config);

// Start emitting
particleComp->play();
```

### Creating Explosions

```cpp
// Create an explosion at the entity's position
auto explosion = entity->addComponent<ParticleComponent>(ParticleEffectType::EXPLOSION);
explosion->burst(300); // Emit 300 particles at once
```

## Future Improvements

1. Add support for complex particle shapes and meshes
2. Implement a particle effect editor with real-time preview
3. Add support for particle attraction and repulsion forces
4. Implement collision detection for particles
5. Add support for particle trails and ribbons
6. Implement particle sound effects for integrated audio-visual effects

## Conclusion

The ParticleComponent provides a flexible and efficient way to add visual effects to entities in the IKore Engine. By wrapping the existing ParticleSystem class, it seamlessly integrates with the Entity-Component System and provides an intuitive API for creating and customizing particle effects.
