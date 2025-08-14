#include "scene/SceneManager.h"
#include "core/Logger.h"
#include "Light.h"
#include <algorithm>

namespace IKore {

    SceneManager& SceneManager::getInstance() {
        static SceneManager instance;
        return instance;
    }

    void SceneManager::initialize() {
        // Logger::getInstance().log(LogLevel::INFO, "SceneManager: Initializing Scene Manager");
        SceneGraphSystem::getInstance().initialize();
    }

    void SceneManager::update(float deltaTime) {
        SceneGraphSystem::getInstance().update(deltaTime);
    }

    void SceneManager::clearScene() {
        // Logger::getInstance().log(LogLevel::INFO, "SceneManager: Clearing scene");
        SceneGraphSystem::getInstance().clear();
    }

    // ============================================================================
    // Entity Creation and Management
    // ============================================================================

    std::shared_ptr<TransformableGameObject> SceneManager::createGameObject(const std::string& name, 
                                                                           std::shared_ptr<Entity> parent) {
        auto gameObject = std::make_shared<TransformableGameObject>(name);
        
        auto parentNode = parent ? SceneGraphSystem::getInstance().findNode(parent) : nullptr;
        SceneGraphSystem::getInstance().addEntity(gameObject, parentNode);
        
        Logger::getInstance().log(LogLevel::INFO, "SceneManager: Created game object '" + name + "' with ID " + std::to_string(gameObject->getID()));
        
        return gameObject;
    }

    std::shared_ptr<TransformableLight> SceneManager::createLight(const std::string& name, 
                                                                 std::shared_ptr<Entity> parent) {
        auto light = std::make_shared<TransformableLight>(name);
        
        auto parentNode = parent ? SceneGraphSystem::getInstance().findNode(parent) : nullptr;
        SceneGraphSystem::getInstance().addEntity(light, parentNode);
        
        Logger::getInstance().log(LogLevel::INFO, "SceneManager: Created light '" + name + "' with ID " + std::to_string(light->getID()));
        
        return light;
    }

    std::shared_ptr<TransformableCamera> SceneManager::createCamera(const std::string& name, 
                                                                   std::shared_ptr<Entity> parent) {
        auto camera = std::make_shared<TransformableCamera>(name);
        
        auto parentNode = parent ? SceneGraphSystem::getInstance().findNode(parent) : nullptr;
        SceneGraphSystem::getInstance().addEntity(camera, parentNode);
        
        Logger::getInstance().log(LogLevel::INFO, "SceneManager: Created camera '" + name + "' with ID " + std::to_string(camera->getID()));
        
        return camera;
    }

    std::shared_ptr<SceneNode> SceneManager::addEntity(std::shared_ptr<Entity> entity, 
                                                      std::shared_ptr<Entity> parent) {
        if (!entity) {
            return nullptr;
        }
        
        auto parentNode = parent ? SceneGraphSystem::getInstance().findNode(parent) : nullptr;
        return SceneGraphSystem::getInstance().addEntity(entity, parentNode);
    }

    bool SceneManager::removeEntity(std::shared_ptr<Entity> entity) {
        if (!entity) {
            return false;
        }
        
        Logger::getInstance().log(LogLevel::INFO, "SceneManager: Removing entity with ID " + std::to_string(entity->getID()));
        
        return SceneGraphSystem::getInstance().removeEntity(entity);
    }

    size_t SceneManager::removeEntitiesByName(const std::string& name) {
        auto entities = findEntitiesByName(name);
        size_t removedCount = 0;
        
        for (auto& entity : entities) {
            if (removeEntity(entity)) {
                removedCount++;
            }
        }
        
        Logger::getInstance().log(LogLevel::INFO, "SceneManager: Removed " + std::to_string(removedCount) + " entities with name '" + name + "'");
        
        return removedCount;
    }

    // ============================================================================
    // Entity Finding and Querying
    // ============================================================================

    std::shared_ptr<Entity> SceneManager::findEntity(EntityID entityID) {
        auto node = SceneGraphSystem::getInstance().findNode(entityID);
        return node ? node->entity : nullptr;
    }

    std::vector<std::shared_ptr<Entity>> SceneManager::findEntitiesByName(const std::string& name) {
        auto nodes = SceneGraphSystem::getInstance().findNodesByName(name);
        std::vector<std::shared_ptr<Entity>> entities;
        
        for (auto& node : nodes) {
            entities.push_back(node->entity);
        }
        
        return entities;
    }

    std::shared_ptr<Entity> SceneManager::findFirstEntityByName(const std::string& name) {
        auto entities = findEntitiesByName(name);
        return entities.empty() ? nullptr : entities.front();
    }

    std::vector<std::shared_ptr<Entity>> SceneManager::getRootEntities() {
        auto rootNodes = SceneGraphSystem::getInstance().getRootNodes();
        std::vector<std::shared_ptr<Entity>> entities;
        
        for (auto& node : rootNodes) {
            entities.push_back(node->entity);
        }
        
        return entities;
    }

    std::vector<std::shared_ptr<Entity>> SceneManager::getAllEntities() {
        auto nodes = SceneGraphSystem::getInstance().getAllNodes();
        std::vector<std::shared_ptr<Entity>> entities;
        
        for (auto& node : nodes) {
            entities.push_back(node->entity);
        }
        
        return entities;
    }

    std::vector<std::shared_ptr<TransformableGameObject>> SceneManager::getAllGameObjects() {
        return getEntitiesOfType<TransformableGameObject>();
    }

    std::vector<std::shared_ptr<TransformableLight>> SceneManager::getAllLights() {
        return getEntitiesOfType<TransformableLight>();
    }

    std::vector<std::shared_ptr<TransformableCamera>> SceneManager::getAllCameras() {
        return getEntitiesOfType<TransformableCamera>();
    }

    // ============================================================================
    // Hierarchy Operations
    // ============================================================================

    bool SceneManager::setParent(std::shared_ptr<Entity> child, std::shared_ptr<Entity> parent) {
        return SceneGraphSystem::getInstance().setParent(child, parent);
    }

    std::vector<std::shared_ptr<Entity>> SceneManager::getChildren(std::shared_ptr<Entity> entity) {
        auto nodes = SceneGraphSystem::getInstance().getChildren(entity);
        std::vector<std::shared_ptr<Entity>> entities;
        
        for (auto& node : nodes) {
            entities.push_back(node->entity);
        }
        
        return entities;
    }

    std::vector<std::shared_ptr<Entity>> SceneManager::getDescendants(std::shared_ptr<Entity> entity) {
        auto nodes = SceneGraphSystem::getInstance().getDescendants(entity);
        std::vector<std::shared_ptr<Entity>> entities;
        
        for (auto& node : nodes) {
            entities.push_back(node->entity);
        }
        
        return entities;
    }

    std::shared_ptr<Entity> SceneManager::getParent(std::shared_ptr<Entity> entity) {
        auto node = SceneGraphSystem::getInstance().findNode(entity);
        if (!node || !node->parent) {
            return nullptr;
        }
        
        return node->parent->entity;
    }

    // ============================================================================
    // Scene Traversal
    // ============================================================================

    void SceneManager::forEachEntity(std::function<void(std::shared_ptr<Entity>)> visitor, bool rootsOnly) {
        SceneGraphSystem::getInstance().traverseDepthFirst([visitor](std::shared_ptr<SceneNode> node) {
            if (node && node->entity) {
                visitor(node->entity);
            }
        }, rootsOnly);
    }

    void SceneManager::forEachGameObject(std::function<void(std::shared_ptr<TransformableGameObject>)> visitor) {
        forEachEntity([visitor](std::shared_ptr<Entity> entity) {
            auto gameObject = std::dynamic_pointer_cast<TransformableGameObject>(entity);
            if (gameObject) {
                visitor(gameObject);
            }
        });
    }

    void SceneManager::forEachLight(std::function<void(std::shared_ptr<TransformableLight>)> visitor) {
        forEachEntity([visitor](std::shared_ptr<Entity> entity) {
            auto light = std::dynamic_pointer_cast<TransformableLight>(entity);
            if (light) {
                visitor(light);
            }
        });
    }

    void SceneManager::forEachCamera(std::function<void(std::shared_ptr<TransformableCamera>)> visitor) {
        forEachEntity([visitor](std::shared_ptr<Entity> entity) {
            auto camera = std::dynamic_pointer_cast<TransformableCamera>(entity);
            if (camera) {
                visitor(camera);
            }
        });
    }

    // ============================================================================
    // Transform Operations
    // ============================================================================

    void SceneManager::updateTransforms() {
        SceneGraphSystem::getInstance().updateTransforms();
    }

    void SceneManager::markTransformDirty(std::shared_ptr<Entity> entity) {
        SceneGraphSystem::getInstance().markTransformDirty(entity);
    }

    // ============================================================================
    // Visibility and State Management
    // ============================================================================

    void SceneManager::setVisibility(std::shared_ptr<Entity> entity, bool visible, bool recursive) {
        auto node = SceneGraphSystem::getInstance().findNode(entity);
        if (node) {
            SceneGraphSystem::getInstance().setVisibility(node, visible, recursive);
        }
    }

    void SceneManager::setActive(std::shared_ptr<Entity> entity, bool active, bool recursive) {
        auto node = SceneGraphSystem::getInstance().findNode(entity);
        if (node) {
            SceneGraphSystem::getInstance().setActive(node, active, recursive);
        }
    }

    std::vector<std::shared_ptr<Entity>> SceneManager::getVisibleEntities() {
        auto nodes = SceneGraphSystem::getInstance().getVisibleNodes();
        std::vector<std::shared_ptr<Entity>> entities;
        
        for (auto& node : nodes) {
            entities.push_back(node->entity);
        }
        
        return entities;
    }

    std::vector<std::shared_ptr<Entity>> SceneManager::getActiveEntities() {
        auto nodes = SceneGraphSystem::getInstance().getActiveNodes();
        std::vector<std::shared_ptr<Entity>> entities;
        
        for (auto& node : nodes) {
            entities.push_back(node->entity);
        }
        
        return entities;
    }

    bool SceneManager::isVisible(std::shared_ptr<Entity> entity) {
        auto node = SceneGraphSystem::getInstance().findNode(entity);
        return node ? node->isVisible : false;
    }

    bool SceneManager::isActive(std::shared_ptr<Entity> entity) {
        auto node = SceneGraphSystem::getInstance().findNode(entity);
        return node ? node->isActive : false;
    }

    // ============================================================================
    // Scene Information and Debug
    // ============================================================================

    void SceneManager::printScene(bool detailed) {
        SceneGraphSystem::getInstance().printSceneGraph(detailed);
    }

    std::string SceneManager::getSceneStats() {
        return SceneGraphSystem::getInstance().getSceneGraphStats();
    }

    size_t SceneManager::getEntityCount() {
        return SceneGraphSystem::getInstance().getNodeCount();
    }

    size_t SceneManager::getMaxDepth() {
        return SceneGraphSystem::getInstance().getMaxDepth();
    }

    // ============================================================================
    // Scene Presets and Utilities
    // ============================================================================

    std::vector<std::shared_ptr<Entity>> SceneManager::createBasicScene() {
        Logger::getInstance().log(LogLevel::INFO, "SceneManager: Creating basic scene");
        
        std::vector<std::shared_ptr<Entity>> entities;
        
        // Create main camera
        auto camera = createCamera("MainCamera");
        camera->getTransform().setPosition(glm::vec3(0.0f, 0.0f, 5.0f));
        entities.push_back(camera);
        
        // Create main light
        auto light = createLight("MainLight");
        light->getTransform().setPosition(glm::vec3(2.0f, 4.0f, 2.0f));
        entities.push_back(light);
        
        // Create root object for organizing scene
        auto sceneRoot = createGameObject("SceneRoot");
        entities.push_back(sceneRoot);
        
        Logger::getInstance().log(LogLevel::INFO, "SceneManager: Created basic scene with " + std::to_string(entities.size()) + " entities");
        
        return entities;
    }

    std::shared_ptr<TransformableGameObject> SceneManager::createHierarchyDemo() {
        Logger::getInstance().log(LogLevel::INFO, "SceneManager: Creating hierarchy demonstration");
        
        // Create parent object
        auto parent = createGameObject("HierarchyParent");
        parent->getTransform().setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        
        // Create child objects
        for (int i = 0; i < 3; ++i) {
            auto child = createGameObject("HierarchyChild_" + std::to_string(i), parent);
            child->getTransform().setPosition(glm::vec3(i * 2.0f, 0.0f, 0.0f));
            
            // Create grandchild for the first child
            if (i == 0) {
                auto grandchild = createGameObject("HierarchyGrandchild", child);
                grandchild->getTransform().setPosition(glm::vec3(0.0f, 2.0f, 0.0f));
            }
        }
        
        Logger::getInstance().log(LogLevel::INFO, "SceneManager: Created hierarchy demonstration");
        
        return parent;
    }

    bool SceneManager::validateSceneIntegrity() {
        Logger::getInstance().log(LogLevel::INFO, "SceneManager: Validating scene integrity");
        
        bool isValid = true;
        auto allNodes = SceneGraphSystem::getInstance().getAllNodes();
        
        for (auto& node : allNodes) {
            // Check that each node's entity exists
            if (!node->entity) {
                Logger::getInstance().log(LogLevel::ERROR, "SceneManager: Found node with null entity");
                isValid = false;
                continue;
            }
            
            // Check parent-child relationships
            if (node->parent) {
                // Check if this node is in parent's children list
                auto& siblings = node->parent->children;
                if (std::find(siblings.begin(), siblings.end(), node) == siblings.end()) {
                    Logger::getInstance().log(LogLevel::ERROR, "SceneManager: Node not found in parent's children list");
                    isValid = false;
                }
            }
            
            // Check children's parent references
            for (auto& child : node->children) {
                if (!child->parent || child->parent != node) {
                    Logger::getInstance().log(LogLevel::ERROR, "SceneManager: Child's parent reference is incorrect");
                    isValid = false;
                }
            }
        }
        
        Logger::getInstance().log(LogLevel::INFO, "SceneManager: Scene integrity validation " + std::string(isValid ? "passed" : "failed"));
        
        return isValid;
    }

    // ============================================================================
    // Helper Methods
    // ============================================================================

    template<typename T>
    std::vector<std::shared_ptr<T>> SceneManager::getEntitiesOfType() {
        std::vector<std::shared_ptr<T>> entities;
        
        forEachEntity([&entities](std::shared_ptr<Entity> entity) {
            auto typedEntity = std::dynamic_pointer_cast<T>(entity);
            if (typedEntity) {
                entities.push_back(typedEntity);
            }
        });
        
        return entities;
    }

    // ============================================================================
    // Convenience Function
    // ============================================================================

    SceneManager& getSceneManager() {
        return SceneManager::getInstance();
    }

} // namespace IKore
