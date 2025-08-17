#include <iostream>
#include <memory>
#include <vector>
#include "src/core/Entity.h"
#include "src/core/components/TransformComponent.h"
#include "src/core/components/PhysicsComponent.h"

using namespace IKore;

int main() {
    std::cout << "Testing PhysicsComponent creation with many entities..." << std::endl;
    
    try {
        std::vector<std::shared_ptr<Entity>> entities;
        
        // Create 250 entities to see if we can reproduce the crash
        for (int i = 0; i < 250; ++i) {
            auto entity = std::make_shared<Entity>();
            
            // Add transform component first
            auto transform = entity->addComponent<TransformComponent>();
            transform->position = glm::vec3(0.0f, 10.0f, 0.0f);
            
            // Add physics component  
            auto physics = entity->addComponent<PhysicsComponent>();
            
            // Initialize the rigid body
            physics->initializeRigidBody(BodyType::DYNAMIC, ColliderType::BOX, 
                                       glm::vec3(1.0f, 1.0f, 1.0f), 1.0f);
            
            entities.push_back(entity);
            
            if ((i + 1) % 50 == 0) {
                std::cout << "Created " << (i + 1) << " entities..." << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
