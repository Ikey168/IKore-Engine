#include "src/core/components/MaterialComponent.h"
#include <iostream>

int main() {
    std::cout << "Simple MaterialComponent test..." << std::endl;
    
    try {
        // Test basic MaterialComponent creation
        auto material = std::make_shared<IKore::MaterialComponent>();
        std::cout << "✓ Created MaterialComponent" << std::endl;
        
        // Test basic color setting
        material->setDiffuseColor(glm::vec3(1.0f, 0.0f, 0.0f));
        std::cout << "✓ Set diffuse color" << std::endl;
        
        // Test material access
        const auto& mat = material->getMaterial();
        std::cout << "✓ Accessed material properties" << std::endl;
        
        std::cout << "✓ All basic tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}
