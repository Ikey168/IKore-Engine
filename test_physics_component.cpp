#include <iostream>
#include <memory>
#include "src/core/Entity.h"
#include "src/core/components/TransformComponent.h"
#include "src/core/components/PhysicsComponent.h"

using namespace IKore;

int main() {
    std::cout << "=== PhysicsComponent Basic Test ===" << std::endl;
    
    try {
        // Create an entity
        auto entity = std::make_shared<Entity>();
        std::cout << "✅ Entity created" << std::endl;
        
        // Add transform component first (required by physics)
        auto transform = entity->addComponent<TransformComponent>();
        transform->position = glm::vec3(0.0f, 10.0f, 0.0f);
        std::cout << "✅ Transform component added" << std::endl;
        
        // Add physics component
        auto physics = entity->addComponent<PhysicsComponent>();
        std::cout << "✅ PhysicsComponent added" << std::endl;
        
        // Test different body types and colliders
        std::cout << "\n--- Testing Different Physics Bodies ---" << std::endl;
        
        // Dynamic box
        physics->initializeRigidBody(BodyType::DYNAMIC, ColliderType::BOX, 
                                   glm::vec3(2.0f, 2.0f, 2.0f), 10.0f);
        std::cout << "✅ Dynamic box initialized" << std::endl;
        
        // Test factory methods
        auto staticBox = PhysicsComponent::createStaticBox(glm::vec3(5.0f, 1.0f, 5.0f));
        std::cout << "✅ Static box factory method works" << std::endl;
        
        auto dynamicSphere = PhysicsComponent::createDynamicSphere(1.5f, 8.0f);
        std::cout << "✅ Dynamic sphere factory method works" << std::endl;
        
        auto characterCapsule = PhysicsComponent::createCharacterCapsule(0.5f, 2.0f, 70.0f);
        std::cout << "✅ Character capsule factory method works" << std::endl;
        
        // Test properties
        std::cout << "\n--- Testing Physics Properties ---" << std::endl;
        
        // Material properties
        physics->setFriction(0.8f);
        physics->setRestitution(0.3f);
        std::cout << "✅ Friction: " << physics->getFriction() << std::endl;
        std::cout << "✅ Restitution: " << physics->getRestitution() << std::endl;
        
        // Test body state
        std::cout << "✅ Is dynamic: " << (physics->isDynamic() ? "true" : "false") << std::endl;
        std::cout << "✅ Is static: " << (physics->isStatic() ? "true" : "false") << std::endl;
        std::cout << "✅ Is initialized: " << (physics->isInitialized() ? "true" : "false") << std::endl;
        
        // Test velocity
        physics->setLinearVelocity(glm::vec3(5.0f, 0.0f, 0.0f));
        auto velocity = physics->getLinearVelocity();
        std::cout << "✅ Linear velocity: (" << velocity.x << ", " << velocity.y << ", " << velocity.z << ")" << std::endl;
        
        // Test position
        auto position = physics->getPosition();
        std::cout << "✅ Physics position: (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
        
        std::cout << "\n=== PhysicsComponent Test PASSED ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
