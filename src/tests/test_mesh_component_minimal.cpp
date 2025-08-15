#include <iostream>
#include <memory>

// Test minimal includes
#include "../src/core/Entity.h"
#include "../src/core/components/MeshComponent.h"

int main() {
    std::cout << "🧪 Testing minimal MeshComponent functionality..." << std::endl;
    
    try {
        // Test 1: Basic entity creation
        std::cout << "Creating entity..." << std::endl;
        auto entity = std::make_shared<IKore::Entity>();
        std::cout << "✅ Entity created successfully" << std::endl;
        
        // Test 2: Add MeshComponent
        std::cout << "Adding MeshComponent..." << std::endl;
        auto meshComp = entity->addComponent<IKore::MeshComponent>();
        std::cout << "✅ MeshComponent added successfully" << std::endl;
        
        // Test 3: Basic state check
        std::cout << "Checking initial state..." << std::endl;
        std::cout << "Has meshes: " << (meshComp->hasMeshes() ? "yes" : "no") << std::endl;
        std::cout << "Mesh count: " << meshComp->getMeshCount() << std::endl;
        std::cout << "✅ Initial state checks passed" << std::endl;
        
        std::cout << "🎉 Minimal test completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown exception occurred" << std::endl;
        return 1;
    }
}
