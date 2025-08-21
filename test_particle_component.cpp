#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

#include "src/core/Entity.h"
#include "src/core/components/ParticleComponent.h"
#include "src/core/components/TransformComponent.h"

using namespace IKore;

int main() {
    std::cout << "==== Particle Component Test ====" << std::endl;
    
    // Create an entity
    auto entity = std::make_shared<Entity>();
    
    // Add a transform component
    auto transform = entity->addComponent<TransformComponent>();
    transform->setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    
    // Add a particle component with fire effect
    auto particleComponent = entity->addComponent<ParticleComponent>(ParticleEffectType::FIRE);
    
    std::cout << "Initialized ParticleComponent with fire effect" << std::endl;
    
    // Initialize the particle component
    if (!particleComponent->initialize(500)) {
        std::cerr << "Failed to initialize particle component" << std::endl;
        return 1;
    }
    
    // Start emitting particles
    particleComponent->play();
    std::cout << "Started particle emission" << std::endl;
    
    // Simulate a game loop for 5 seconds
    const float deltaTime = 0.016f; // ~60 fps
    for (int i = 0; i < 300; i++) {
        // Update particle system
        particleComponent->update(deltaTime);
        
        if (i % 60 == 0) {
            std::cout << "Update frame " << i << ", active particles: " 
                      << particleComponent->getActiveParticles() << std::endl;
        }
        
        // Move the entity to test position updates
        if (i == 100) {
            transform->setPosition(glm::vec3(1.0f, 2.0f, 3.0f));
            std::cout << "Moved entity to (1, 2, 3)" << std::endl;
        }
        
        // Change effect type midway to test switching
        if (i == 200) {
            particleComponent->setEffectType(ParticleEffectType::SMOKE);
            std::cout << "Changed effect to smoke" << std::endl;
        }
        
        // Simulate frame timing
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    // Test stopping and restarting
    particleComponent->stop();
    std::cout << "Stopped particle emission" << std::endl;
    
    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Restart with explosion
    particleComponent->setEffectType(ParticleEffectType::EXPLOSION);
    particleComponent->play();
    std::cout << "Restarted with explosion effect" << std::endl;
    
    // One more burst
    particleComponent->burst(100);
    std::cout << "Added burst of 100 particles" << std::endl;
    
    // A few more updates
    for (int i = 0; i < 60; i++) {
        particleComponent->update(deltaTime);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    // Remove component
    entity->removeComponent<ParticleComponent>();
    std::cout << "Removed particle component" << std::endl;
    
    std::cout << "Particle Component test completed successfully" << std::endl;
    return 0;
}
