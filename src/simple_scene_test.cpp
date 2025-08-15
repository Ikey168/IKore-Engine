#include "Entity.h"
#include "TransformableEntities.h"
#include <iostream>
#include <memory>

using namespace IKore;

int main() {
    try {
        std::cout << "Testing basic Entity and Transform functionality..." << std::endl;
        
        // Test basic entity creation
        auto entity = std::make_shared<TransformableGameObject>("TestEntity");
        std::cout << "Created TransformableGameObject: " << entity->getName() << std::endl;
        std::cout << "Entity ID: " << entity->getID() << std::endl;
        
        // Test transform functionality
        entity->getTransform().setPosition(glm::vec3(1.0f, 2.0f, 3.0f));
        auto pos = entity->getTransform().getPosition();
        std::cout << "Set position to (1, 2, 3), got: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
        
        // Test hierarchy
        auto child = std::make_shared<TransformableGameObject>("ChildEntity");
        child->setParent(entity.get());
        std::cout << "Created child entity and set parent relationship" << std::endl;
        
        // Test parent-child relationship
        auto children = entity->getChildren();
        std::cout << "Parent has " << children.size() << " children" << std::endl;
        
        if (!children.empty()) {
            auto childObj = dynamic_cast<TransformableGameObject*>(children[0]);
            if (childObj) {
                std::cout << "Child name: " << childObj->getName() << std::endl;
            }
        }
        
        std::cout << "Basic entity and transform test completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception occurred" << std::endl;
        return 1;
    }
}
