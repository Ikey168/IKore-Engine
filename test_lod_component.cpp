#include <iostream>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Mock classes for testing
class Model {
public:
    Model() = default;
    Model(const std::string& path) : path_(path) {
        std::cout << "Loading model: " << path << std::endl;
    }
    
    bool loadFromFile(const std::string& path) {
        path_ = path;
        std::cout << "Loading model: " << path << std::endl;
        return true; // Mock success
    }
    
    const std::string& getPath() const { return path_; }
private:
    std::string path_;
};

class Component {
public:
    virtual ~Component() = default;
    virtual void onAttach() {}
    virtual void onDetach() {}
    std::weak_ptr<class Entity> entity_;
    std::weak_ptr<class Entity> getEntity() const { return entity_; }
};

class TransformComponent : public Component {
public:
    void setPosition(const glm::vec3& pos) { position_ = pos; }
    glm::vec3 getPosition() const { return position_; }
private:
    glm::vec3 position_{0.0f};
};

class Entity : public std::enable_shared_from_this<Entity> {
public:
    template<typename T>
    std::shared_ptr<T> getComponent() {
        for (auto& comp : components_) {
            auto typed = std::dynamic_pointer_cast<T>(comp);
            if (typed) return typed;
        }
        return nullptr;
    }
    
    template<typename T>
    std::shared_ptr<T> addComponent() {
        auto comp = std::make_shared<T>();
        comp->entity_ = shared_from_this();
        components_.push_back(comp);
        comp->onAttach();
        return comp;
    }

private:
    std::vector<std::shared_ptr<Component>> components_;
};

// Include the LOD component
#include "src/core/components/LODComponent.h"

using namespace IKore;

int main() {
    std::cout << "=== LOD Component Test ===" << std::endl;
    
    // Create entity with transform and LOD components
    auto entity = std::make_shared<Entity>();
    auto transform = entity->addComponent<TransformComponent>();
    auto lod = entity->addComponent<LODComponent>();
    
    // Set entity position
    transform->setPosition(glm::vec3(0, 0, 0));
    
    // Configure LOD levels
    std::cout << "\n--- Configuring LOD Levels ---" << std::endl;
    lod->addLODLevel(LODLevel::LOD_0, 0.0f, "models/high_detail.obj", 10000);
    lod->addLODLevel(LODLevel::LOD_1, 50.0f, "models/medium_detail.obj", 5000);
    lod->addLODLevel(LODLevel::LOD_2, 100.0f, "models/low_detail.obj", 2500);
    lod->addLODLevel(LODLevel::LOD_3, 200.0f, "models/very_low_detail.obj", 1000);
    lod->addLODLevel(LODLevel::LOD_4, 500.0f, "models/billboard.obj", 2);
    
    // Initialize component
    std::cout << "\n--- Initializing LOD Component ---" << std::endl;
    if (!lod->initialize()) {
        std::cout << "Failed to initialize LOD component!" << std::endl;
        return 1;
    }
    
    // Test different camera distances
    std::cout << "\n--- Testing Distance-Based LOD Switching ---" << std::endl;
    std::vector<float> testDistances = {10.0f, 75.0f, 150.0f, 300.0f, 600.0f};
    
    for (float distance : testDistances) {
        glm::vec3 cameraPos(distance, 0, 0);
        lod->update(cameraPos, 0.016f); // 60 FPS
        
        std::cout << "Distance: " << distance << " -> LOD Level: " 
                  << static_cast<int>(lod->getCurrentLOD()) 
                  << " (Triangles: " << lod->getCurrentTriangleCount() << ")" << std::endl;
    }
    
    // Test forced LOD
    std::cout << "\n--- Testing Forced LOD ---" << std::endl;
    lod->forceLODLevel(LODLevel::LOD_2);
    std::cout << "Forced LOD to level 2: " << static_cast<int>(lod->getCurrentLOD()) << std::endl;
    
    lod->enableAutoLOD();
    lod->update(glm::vec3(10, 0, 0), 0.016f);
    std::cout << "After clearing forced LOD (distance 10): " << static_cast<int>(lod->getCurrentLOD()) << std::endl;
    
    // Test hysteresis
    std::cout << "\n--- Testing Hysteresis ---" << std::endl;
    lod->setHysteresis(0.2f); // 20% hysteresis
    std::cout << "Set hysteresis to 20%" << std::endl;
    
    // Test performance stats
    std::cout << "\n--- Performance Statistics ---" << std::endl;
    std::cout << lod->getPerformanceStats() << std::endl;
    
    std::cout << "\n=== LOD Component Test Complete ===" << std::endl;
    return 0;
}
