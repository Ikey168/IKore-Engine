#pragma once

#include "scene/SceneGraphSystem.h"
#include "core/Entity.h"
#include "core/TransformableEntities.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace IKore {

    /**
     * @brief High-level scene management utility
     * 
     * Provides convenient methods for common scene operations and management tasks.
     * Built on top of the SceneGraphSystem for comprehensive scene manipulation.
     */
    class SceneManager {
    public:
        /**
         * @brief Get the singleton instance
         * @return Reference to the SceneManager instance
         */
        static SceneManager& getInstance();

        /**
         * @brief Initialize the scene manager
         */
        void initialize();

        /**
         * @brief Update the scene (called once per frame)
         * @param deltaTime Time since last frame in seconds
         */
        void update(float deltaTime);

        /**
         * @brief Clear the entire scene
         */
        void clearScene();

        // ============================================================================
        // Entity Creation and Management
        // ============================================================================

        /**
         * @brief Create a new transformable game object in the scene
         * @param name Name for the entity
         * @param parent Parent entity (nullptr for root)
         * @return Shared pointer to created entity
         */
        std::shared_ptr<TransformableGameObject> createGameObject(const std::string& name, 
                                                                 std::shared_ptr<Entity> parent = nullptr);

        /**
         * @brief Create a new transformable light in the scene
         * @param name Name for the light
         * @param parent Parent entity (nullptr for root)
         * @return Shared pointer to created light
         */
        std::shared_ptr<TransformableLight> createLight(const std::string& name, 
                                                       std::shared_ptr<Entity> parent = nullptr);

        /**
         * @brief Create a new transformable camera in the scene
         * @param name Name for the camera
         * @param parent Parent entity (nullptr for root)
         * @return Shared pointer to created camera
         */
        std::shared_ptr<TransformableCamera> createCamera(const std::string& name, 
                                                         std::shared_ptr<Entity> parent = nullptr);

        /**
         * @brief Add an existing entity to the scene
         * @param entity Entity to add
         * @param parent Parent entity (nullptr for root)
         * @return Scene node for the added entity
         */
        std::shared_ptr<SceneNode> addEntity(std::shared_ptr<Entity> entity, 
                                           std::shared_ptr<Entity> parent = nullptr);

        /**
         * @brief Remove an entity from the scene
         * @param entity Entity to remove
         * @return True if entity was removed successfully
         */
        bool removeEntity(std::shared_ptr<Entity> entity);

        /**
         * @brief Remove an entity by name
         * @param name Name of entity to remove
         * @return Number of entities removed
         */
        size_t removeEntitiesByName(const std::string& name);

        // ============================================================================
        // Entity Finding and Querying
        // ============================================================================

        /**
         * @brief Find an entity by ID
         * @param entityID Entity ID to find
         * @return Shared pointer to entity (nullptr if not found)
         */
        std::shared_ptr<Entity> findEntity(EntityID entityID);

        /**
         * @brief Find entities by name
         * @param name Name to search for
         * @return Vector of matching entities
         */
        std::vector<std::shared_ptr<Entity>> findEntitiesByName(const std::string& name);

        /**
         * @brief Find the first entity with the given name
         * @param name Name to search for
         * @return Shared pointer to first matching entity (nullptr if not found)
         */
        std::shared_ptr<Entity> findFirstEntityByName(const std::string& name);

        /**
         * @brief Get all root entities (entities without parents)
         * @return Vector of root entities
         */
        std::vector<std::shared_ptr<Entity>> getRootEntities();

        /**
         * @brief Get all entities in the scene
         * @return Vector of all entities
         */
        std::vector<std::shared_ptr<Entity>> getAllEntities();

        /**
         * @brief Get all transformable game objects in the scene
         * @return Vector of transformable game objects
         */
        std::vector<std::shared_ptr<TransformableGameObject>> getAllGameObjects();

        /**
         * @brief Get all lights in the scene
         * @return Vector of transformable lights
         */
        std::vector<std::shared_ptr<TransformableLight>> getAllLights();

        /**
         * @brief Get all cameras in the scene
         * @return Vector of transformable cameras
         */
        std::vector<std::shared_ptr<TransformableCamera>> getAllCameras();

        // ============================================================================
        // Hierarchy Operations
        // ============================================================================

        /**
         * @brief Set parent-child relationship between entities
         * @param child Child entity
         * @param parent Parent entity (nullptr to make root)
         * @return True if relationship was established successfully
         */
        bool setParent(std::shared_ptr<Entity> child, std::shared_ptr<Entity> parent);

        /**
         * @brief Get children of an entity
         * @param entity Parent entity
         * @return Vector of child entities
         */
        std::vector<std::shared_ptr<Entity>> getChildren(std::shared_ptr<Entity> entity);

        /**
         * @brief Get all descendants of an entity (recursive)
         * @param entity Root entity
         * @return Vector of descendant entities
         */
        std::vector<std::shared_ptr<Entity>> getDescendants(std::shared_ptr<Entity> entity);

        /**
         * @brief Get the parent of an entity
         * @param entity Child entity
         * @return Shared pointer to parent (nullptr if root)
         */
        std::shared_ptr<Entity> getParent(std::shared_ptr<Entity> entity);

        // ============================================================================
        // Scene Traversal
        // ============================================================================

        /**
         * @brief Visit all entities in the scene depth-first
         * @param visitor Function to call for each entity
         * @param rootsOnly If true, only traverse from root entities
         */
        void forEachEntity(std::function<void(std::shared_ptr<Entity>)> visitor, bool rootsOnly = true);

        /**
         * @brief Visit all transformable game objects in the scene
         * @param visitor Function to call for each game object
         */
        void forEachGameObject(std::function<void(std::shared_ptr<TransformableGameObject>)> visitor);

        /**
         * @brief Visit all lights in the scene
         * @param visitor Function to call for each light
         */
        void forEachLight(std::function<void(std::shared_ptr<TransformableLight>)> visitor);

        /**
         * @brief Visit all cameras in the scene
         * @param visitor Function to call for each camera
         */
        void forEachCamera(std::function<void(std::shared_ptr<TransformableCamera>)> visitor);

        // ============================================================================
        // Transform Operations
        // ============================================================================

        /**
         * @brief Update all transforms in the scene
         */
        void updateTransforms();

        /**
         * @brief Mark an entity's transform as dirty for efficient updates
         * @param entity Entity whose transform changed
         */
        void markTransformDirty(std::shared_ptr<Entity> entity);

        // ============================================================================
        // Visibility and State Management
        // ============================================================================

        /**
         * @brief Set visibility of an entity and optionally its children
         * @param entity Entity to modify
         * @param visible Visibility state
         * @param recursive Apply to children as well
         */
        void setVisibility(std::shared_ptr<Entity> entity, bool visible, bool recursive = false);

        /**
         * @brief Set active state of an entity and optionally its children
         * @param entity Entity to modify
         * @param active Active state
         * @param recursive Apply to children as well
         */
        void setActive(std::shared_ptr<Entity> entity, bool active, bool recursive = false);

        /**
         * @brief Get all visible entities in the scene
         * @return Vector of visible entities
         */
        std::vector<std::shared_ptr<Entity>> getVisibleEntities();

        /**
         * @brief Get all active entities in the scene
         * @return Vector of active entities
         */
        std::vector<std::shared_ptr<Entity>> getActiveEntities();

        /**
         * @brief Check if an entity is visible
         * @param entity Entity to check
         * @return True if entity is visible
         */
        bool isVisible(std::shared_ptr<Entity> entity);

        /**
         * @brief Check if an entity is active
         * @param entity Entity to check
         * @return True if entity is active
         */
        bool isActive(std::shared_ptr<Entity> entity);

        // ============================================================================
        // Scene Information and Debug
        // ============================================================================

        /**
         * @brief Print the scene hierarchy to console
         * @param detailed Include detailed entity information
         */
        void printScene(bool detailed = false);

        /**
         * @brief Get scene statistics
         * @return String containing scene statistics
         */
        std::string getSceneStats();

        /**
         * @brief Get the total number of entities in the scene
         * @return Entity count
         */
        size_t getEntityCount();

        /**
         * @brief Get the maximum depth of the scene hierarchy
         * @return Maximum depth
         */
        size_t getMaxDepth();

        // ============================================================================
        // Scene Presets and Utilities
        // ============================================================================

        /**
         * @brief Create a basic scene with default lighting
         * @return Vector of created entities (camera, light, etc.)
         */
        std::vector<std::shared_ptr<Entity>> createBasicScene();

        /**
         * @brief Create a simple hierarchy demonstration
         * @return Root entity of the created hierarchy
         */
        std::shared_ptr<TransformableGameObject> createHierarchyDemo();

        /**
         * @brief Validate scene graph integrity
         * @return True if scene graph is valid
         */
        bool validateSceneIntegrity();

    private:
        SceneManager() = default;
        ~SceneManager() = default;
        SceneManager(const SceneManager&) = delete;
        SceneManager& operator=(const SceneManager&) = delete;

        // Helper methods
        template<typename T>
        std::vector<std::shared_ptr<T>> getEntitiesOfType();
    };

    /**
     * @brief Convenience function to get the scene manager instance
     * @return Reference to the SceneManager
     */
    SceneManager& getSceneManager();

} // namespace IKore
