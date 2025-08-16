#include <iostream>
#include <memory>

#include "../src/core/Entity.h"
#include "../src/core/components/MeshComponent.h"

int main() {
    std::cout << "🧪 Testing MeshComponent geometry generation..." << std::endl;
    
    try {
        auto entity = std::make_shared<IKore::Entity>();
        auto meshComp = entity->addComponent<IKore::MeshComponent>();
        
        std::cout << "Testing cube generation..." << std::endl;
        auto cubeComp = IKore::MeshComponent::createCube(1.0f);
        std::cout << "✅ Cube created successfully" << std::endl;
        
        std::cout << "Cube has meshes: " << cubeComp->hasMeshes() << std::endl;
        std::cout << "Cube mesh count: " << cubeComp->getMeshCount() << std::endl;
        
        if (cubeComp->hasMeshes()) {
            const auto* mesh = cubeComp->getPrimaryMesh();
            std::cout << "Cube vertices: " << mesh->getVertices().size() << std::endl;
            std::cout << "Cube indices: " << mesh->getIndices().size() << std::endl;
        }
        
        std::cout << "🎉 Geometry generation test completed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown exception occurred" << std::endl;
        return 1;
    }
}
