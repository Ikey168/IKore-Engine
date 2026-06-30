#include <iostream>
#include <memory>
#include <vector>
#include "src/core/Entity.h"
#include "src/core/components/TransformComponent.h"
#include "src/core/components/VelocityComponent.h"
#include "src/core/components/RenderableComponent.h"

using namespace IKore;

std::vector<std::shared_ptr<Entity>> createTestEntities(size_t count) {
    std::vector<std::shared_ptr<Entity>> entities;
    entities.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        entities.push_back(std::make_shared<Entity>());
    }
    
    return entities;
}

int main() {
    std::cout << "Testing entity and component creation like ECS benchmark..." << std::endl;
    
    size_t entityCount = 100;
    
    // Step 1: benchmarkEntityOperations equivalent
    std::cout << "\n=== Step 1: Entity Operations ===" << std::endl;
    std::vector<std::shared_ptr<Entity>> entities1;
    entities1.reserve(entityCount);
    
    for (size_t i = 0; i < entityCount; ++i) {
        entities1.push_back(std::make_shared<Entity>());
    }
    std::cout << "Created " << entityCount << " entities for entity operations" << std::endl;
    entities1.clear(); // Cleanup like the benchmark does
    
    // Step 2: benchmarkComponentOperations equivalent  
    std::cout << "\n=== Step 2: Component Operations ===" << std::endl;
    auto entities = createTestEntities(entityCount);
    std::cout << "Created " << entityCount << " entities for component operations" << std::endl;
    
    // Add components like the benchmark does
    for (auto& entity : entities) {
        std::cout << "Adding TransformComponent to entity " << entity->getID() << "..." << std::endl;
        entity->addComponent<TransformComponent>();
        
        std::cout << "Adding VelocityComponent to entity " << entity->getID() << "..." << std::endl;
        entity->addComponent<VelocityComponent>();
        
        std::cout << "Adding RenderableComponent to entity " << entity->getID() << "..." << std::endl;
        entity->addComponent<RenderableComponent>();
    }
    
    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
