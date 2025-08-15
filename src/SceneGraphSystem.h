#pragma once

#include "Entity.h"
#include "TransformableEntities.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <string>

namespace IKore {

    /**
     * @brief Scene graph node representing an entity in the hierarchy
     */
    struct SceneNode {
        std::shared_ptr<Entity> entity;
        std::shared_ptr<SceneNode> parent;
        std::vector<std::shared_ptr<SceneNode>> children;
        bool isActive = true;
        bool isVisible = true;
        
        SceneNode(std::shared_ptr<Entity> ent) : entity(ent) {}
    };

    /**
     * @brief Scene Graph System for managing entity hierarchies
     * 
     * This system provides comprehensive scene graph functionality including:
     * - Hierarchical entity management
     * - Efficient traversal and queries
     * - Transform propagation
     * - Scene manipulation utilities
     */
    class SceneGraphSystem {
    public:
        /**
         * @brief Get the singleton instance
         * @return Reference to the SceneGraphSystem instance
         */
        static SceneGraphSystem& getInstance();

        /**
         * @brief Initialize the scene graph system
         */
        void initialize();

        /**
         * @brief Update the scene graph (called once per frame)
         * @param deltaTime Time since last frame in seconds
         */
        void update(float deltaTime);

        /**
         * @brief Clear the entire scene graph
         */
        void clear();

        // ============================================================================
        // Node Management
        // ============================================================================

        /**
         * @brief Add an entity to the scene graph
         * @param entity Entity to add
         * @param parent Parent node (nullptr for root)
         * @return Shared pointer to the created scene node
         */
        std::shared_ptr<SceneNode> addEntity(std::shared_ptr<Entity> entity, 
                                           std::shared_ptr<SceneNode> parent = nullptr);

        /**
         * @brief Remove an entity from the scene graph
         * @param entity Entity to remove
         * @return True if entity was found and removed
         */
        bool removeEntity(std::shared_ptr<Entity> entity);

        /**
         * @brief Remove a scene node from the scene graph
         * @param node Node to remove
         * @return True if node was found and removed
         */
        bool removeNode(std::shared_ptr<SceneNode> node);

        /**
         * @brief Find a scene node by entity
         * @param entity Entity to find
         * @return Shared pointer to scene node (nullptr if not found)
         */
        std::shared_ptr<SceneNode> findNode(std::shared_ptr<Entity> entity);

        /**
         * @brief Find a scene node by entity ID
         * @param entityID Entity ID to find
         * @return Shared pointer to scene node (nullptr if not found)
         */
        std::shared_ptr<SceneNode> findNode(EntityID entityID);

        /**
         * @brief Find scene nodes by name (searches transformable entities)
         * @param name Name to search for
         * @return Vector of matching scene nodes
         */
        std::vector<std::shared_ptr<SceneNode>> findNodesByName(const std::string& name);

        // ============================================================================
        // Hierarchy Management
        // ============================================================================

        /**
         * @brief Set parent-child relationship between entities
         * @param child Child entity
         * @param parent Parent entity (nullptr to make root)
         * @return True if relationship was established successfully
         */
        bool setParent(std::shared_ptr<Entity> child, std::shared_ptr<Entity> parent);

        /**
         * @brief Set parent-child relationship between nodes
         * @param child Child node
         * @param parent Parent node (nullptr to make root)
         * @return True if relationship was established successfully
         */
        bool setParent(std::shared_ptr<SceneNode> child, std::shared_ptr<SceneNode> parent);

        /**
         * @brief Get all root nodes (nodes without parents)
         * @return Vector of root scene nodes
         */
        const std::vector<std::shared_ptr<SceneNode>>& getRootNodes() const { return m_rootNodes; }

        /**
         * @brief Get all nodes in the scene graph
         * @return Vector of all scene nodes
         */
        std::vector<std::shared_ptr<SceneNode>> getAllNodes() const;

        /**
         * @brief Get children of a specific entity
         * @param entity Parent entity
         * @return Vector of child scene nodes
         */
        std::vector<std::shared_ptr<SceneNode>> getChildren(std::shared_ptr<Entity> entity);

        /**
         * @brief Get all descendants of a specific entity (recursive)
         * @param entity Root entity
         * @return Vector of descendant scene nodes
         */
        std::vector<std::shared_ptr<SceneNode>> getDescendants(std::shared_ptr<Entity> entity);

        // ============================================================================
        // Traversal Methods
        // ============================================================================

        /**
         * @brief Traverse the scene graph depth-first
         * @param visitor Function to call for each node
         * @param rootsOnly If true, only traverse from root nodes
         */
        void traverseDepthFirst(std::function<void(std::shared_ptr<SceneNode>)> visitor, 
                               bool rootsOnly = true);

        /**
         * @brief Traverse the scene graph breadth-first
         * @param visitor Function to call for each node
         * @param rootsOnly If true, only traverse from root nodes
         */
        void traverseBreadthFirst(std::function<void(std::shared_ptr<SceneNode>)> visitor, 
                                 bool rootsOnly = true);

        /**
         * @brief Traverse from a specific node depth-first
         * @param startNode Node to start traversal from
         * @param visitor Function to call for each node
         */
        void traverseFromNode(std::shared_ptr<SceneNode> startNode, 
                             std::function<void(std::shared_ptr<SceneNode>)> visitor);

        // ============================================================================
        // Transform Updates
        // ============================================================================

        /**
         * @brief Update transforms for all entities in the hierarchy
         * Forces recalculation of world transforms
         */
        void updateTransforms();

        /**
         * @brief Update transforms starting from a specific node
         * @param startNode Node to start update from
         */
        void updateTransformsFromNode(std::shared_ptr<SceneNode> startNode);

        /**
         * @brief Mark transform hierarchy as dirty for efficient updates
         * @param entity Entity whose transform changed
         */
        void markTransformDirty(std::shared_ptr<Entity> entity);

        // ============================================================================
        // Visibility and State Management
        // ============================================================================

        /**
         * @brief Set visibility of a node and optionally its children
         * @param node Scene node to modify
         * @param visible Visibility state
         * @param recursive Apply to children as well
         */
        void setVisibility(std::shared_ptr<SceneNode> node, bool visible, bool recursive = false);

        /**
         * @brief Set active state of a node and optionally its children
         * @param node Scene node to modify
         * @param active Active state
         * @param recursive Apply to children as well
         */
        void setActive(std::shared_ptr<SceneNode> node, bool active, bool recursive = false);

        /**
         * @brief Get all visible nodes in the scene graph
         * @return Vector of visible scene nodes
         */
        std::vector<std::shared_ptr<SceneNode>> getVisibleNodes();

        /**
         * @brief Get all active nodes in the scene graph
         * @return Vector of active scene nodes
         */
        std::vector<std::shared_ptr<SceneNode>> getActiveNodes();

        // ============================================================================
        // Utility Methods
        // ============================================================================

        /**
         * @brief Get the depth of a node in the hierarchy
         * @param node Scene node to check
         * @return Depth (0 for root nodes)
         */
        size_t getNodeDepth(std::shared_ptr<SceneNode> node);

        /**
         * @brief Check if one node is an ancestor of another
         * @param ancestor Potential ancestor node
         * @param descendant Potential descendant node
         * @return True if ancestor is a parent/grandparent/etc. of descendant
         */
        bool isAncestorOf(std::shared_ptr<SceneNode> ancestor, std::shared_ptr<SceneNode> descendant);

        /**
         * @brief Get the path from root to a specific node
         * @param node Target node
         * @return Vector of nodes from root to target (inclusive)
         */
        std::vector<std::shared_ptr<SceneNode>> getPathToNode(std::shared_ptr<SceneNode> node);

        /**
         * @brief Print the scene graph structure to console
         * @param detailed Include detailed entity information
         */
        void printSceneGraph(bool detailed = false);

        /**
         * @brief Get scene graph statistics
         * @return String containing graph statistics
         */
        std::string getSceneGraphStats();

        // ============================================================================
        // Performance Monitoring
        // ============================================================================

        /**
         * @brief Get the number of nodes in the scene graph
         * @return Total node count
         */
        size_t getNodeCount() const { return m_nodeMap.size(); }

        /**
         * @brief Get the number of root nodes
         * @return Root node count
         */
        size_t getRootNodeCount() const { return m_rootNodes.size(); }

        /**
         * @brief Get the maximum depth of the scene graph
         * @return Maximum depth
         */
        size_t getMaxDepth();

    private:
        SceneGraphSystem() = default;
        ~SceneGraphSystem() = default;
        SceneGraphSystem(const SceneGraphSystem&) = delete;
        SceneGraphSystem& operator=(const SceneGraphSystem&) = delete;

        // Core data structures
        std::vector<std::shared_ptr<SceneNode>> m_rootNodes;
        std::unordered_map<EntityID, std::shared_ptr<SceneNode>> m_nodeMap;
        std::unordered_set<EntityID> m_dirtyTransforms;

        // Helper methods
        void removeNodeFromParent(std::shared_ptr<SceneNode> node);
        void addNodeToParent(std::shared_ptr<SceneNode> child, std::shared_ptr<SceneNode> parent);
        void updateTransformRecursive(std::shared_ptr<SceneNode> node);
        void collectVisibleNodes(std::shared_ptr<SceneNode> node, 
                                std::vector<std::shared_ptr<SceneNode>>& result);
        void collectActiveNodes(std::shared_ptr<SceneNode> node, 
                               std::vector<std::shared_ptr<SceneNode>>& result);
        void printNodeRecursive(std::shared_ptr<SceneNode> node, int depth, bool detailed);
        size_t calculateMaxDepth(std::shared_ptr<SceneNode> node, size_t currentDepth = 0);
    };

    /**
     * @brief Convenience function to get the scene graph system instance
     * @return Reference to the SceneGraphSystem
     */
    SceneGraphSystem& getSceneGraphSystem();

} // namespace IKore
