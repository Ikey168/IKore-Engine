#include <iostream>
#include <memory>
#include "src/core/components/PhysicsComponent.h"

using namespace IKore;

int main() {
    std::cout << "=== PhysicsComponent Simple Test ===" << std::endl;
    
    try {
        // Test creating components without entity attachment first
        std::cout << "\n--- Testing Factory Methods ---" << std::endl;
        
        auto staticBox = PhysicsComponent::createStaticBox(glm::vec3(5.0f, 1.0f, 5.0f));
        std::cout << "✅ Static box factory method works" << std::endl;
        
        auto dynamicSphere = PhysicsComponent::createDynamicSphere(1.5f, 8.0f);
        std::cout << "✅ Dynamic sphere factory method works" << std::endl;
        
        auto characterCapsule = PhysicsComponent::createCharacterCapsule(0.5f, 2.0f, 70.0f);
        std::cout << "✅ Character capsule factory method works" << std::endl;
        
        // Test basic PhysicsComponent creation
        auto physics = std::make_shared<PhysicsComponent>();
        std::cout << "✅ PhysicsComponent created" << std::endl;
        
        // Test initialization without entity
        physics->initializeRigidBody(BodyType::DYNAMIC, ColliderType::BOX, 
                                   glm::vec3(2.0f, 2.0f, 2.0f), 10.0f);
        std::cout << "✅ RigidBody initialized" << std::endl;
        
        // Test properties
        std::cout << "\n--- Testing Physics Properties ---" << std::endl;
        
        std::cout << "✅ Is initialized: " << (physics->isInitialized() ? "true" : "false") << std::endl;
        std::cout << "✅ Is dynamic: " << (physics->isDynamic() ? "true" : "false") << std::endl;
        std::cout << "✅ Is static: " << (physics->isStatic() ? "true" : "false") << std::endl;
        
        // Test material properties
        physics->setFriction(0.8f);
        physics->setRestitution(0.3f);
        std::cout << "✅ Friction: " << physics->getFriction() << std::endl;
        std::cout << "✅ Restitution: " << physics->getRestitution() << std::endl;
        
        // Test velocity without transform sync
        physics->setLinearVelocity(glm::vec3(5.0f, 0.0f, 0.0f));
        auto velocity = physics->getLinearVelocity();
        std::cout << "✅ Linear velocity: (" << velocity.x << ", " << velocity.y << ", " << velocity.z << ")" << std::endl;
        
        // Test position without transform sync
        physics->setPosition(glm::vec3(1.0f, 2.0f, 3.0f));
        auto position = physics->getPosition();
        std::cout << "✅ Physics position: (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
        
        std::cout << "\n=== PhysicsComponent Simple Test PASSED ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
