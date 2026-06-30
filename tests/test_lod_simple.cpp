#include <iostream>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

// Include logger
#include "src/core/Logger.h"

// Include LOD component
#include "src/core/components/LODComponent.h"

using namespace IKore;

// Simple mock Model class
class Model {
public:
    Model() = default;
    Model(const std::string& path) : path_(path) {
        LOG_INFO("Loading model: " + path);
    }
    
    bool loadFromFile(const std::string& path) {
        path_ = path;
        LOG_INFO("Loading model: " + path);
        return true; // Mock success
    }
    
    const std::string& getPath() const { return path_; }
private:
    std::string path_;
};

// Base Component class
class Component {
public:
    virtual ~Component() = default;
    virtual void onAttach() {}
    virtual void onDetach() {}
    std::weak_ptr<class Entity> entity_;
    std::weak_ptr<class Entity> getEntity() const { return entity_; }
};

// Transform component
class TransformComponent : public Component {
public:
    void setPosition(const glm::vec3& pos) { position_ = pos; }
    glm::vec3 getPosition() const { return position_; }
private:
    glm::vec3 position_{0.0f};
};

// Simple Entity class
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

int main() {
    // Initialize logger
    Logger::getInstance().initialize();
    
    LOG_INFO("=== LOD Component Test ===");
    
    // Create entity with transform and LOD components
    auto entity = std::make_shared<Entity>();
    auto transform = entity->addComponent<TransformComponent>();
    auto lod = entity->addComponent<LODComponent>();
    
    // Set entity position
    transform->setPosition(glm::vec3(0, 0, 0));
    
    // Configure LOD levels
    LOG_INFO("--- Configuring LOD Levels ---");
    lod->addLODLevel(LODLevel::LOD_0, 0.0f, "models/high_detail.obj", 10000);
    lod->addLODLevel(LODLevel::LOD_1, 50.0f, "models/medium_detail.obj", 5000);
    lod->addLODLevel(LODLevel::LOD_2, 100.0f, "models/low_detail.obj", 2500);
    lod->addLODLevel(LODLevel::LOD_3, 200.0f, "models/very_low_detail.obj", 1000);
    lod->addLODLevel(LODLevel::LOD_4, 500.0f, "models/billboard.obj", 2);
    
    // Initialize component
    LOG_INFO("--- Initializing LOD Component ---");
    if (!lod->initialize()) {
        LOG_ERROR("Failed to initialize LOD component!");
        return 1;
    }
    
    // Test different camera distances
    LOG_INFO("--- Testing Distance-Based LOD Switching ---");
    std::vector<float> testDistances = {10.0f, 75.0f, 150.0f, 300.0f, 600.0f};
    
    for (float distance : testDistances) {
        glm::vec3 cameraPos(distance, 0, 0);
        lod->update(cameraPos, 0.016f); // 60 FPS
        
        std::string logMsg = "Distance: " + std::to_string(distance) + 
                           " -> LOD Level: " + std::to_string(static_cast<int>(lod->getCurrentLOD())) +
                           " (Triangles: " + std::to_string(lod->getCurrentTriangleCount()) + ")";
        LOG_INFO(logMsg);
    }
    
    // Test forced LOD
    LOG_INFO("--- Testing Forced LOD ---");
    lod->forceLODLevel(LODLevel::LOD_2);
    LOG_INFO("Forced LOD to level 2: " + std::to_string(static_cast<int>(lod->getCurrentLOD())));
    
    lod->enableAutoLOD();
    lod->update(glm::vec3(10, 0, 0), 0.016f);
    LOG_INFO("After clearing forced LOD (distance 10): " + std::to_string(static_cast<int>(lod->getCurrentLOD())));
    
    // Test performance stats
    LOG_INFO("--- Performance Statistics ---");
    LOG_INFO(lod->getPerformanceStats());
    
    LOG_INFO("=== LOD Component Test Complete ===");
    
    Logger::getInstance().shutdown();
    return 0;
}
