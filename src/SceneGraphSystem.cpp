#include "SceneGraphSystem.h"
#include "Logger.h"
#include <algorithm>
#include <queue>
#include <iostream>
#include <sstream>

namespace IKore {

    SceneGraphSystem& SceneGraphSystem::getInstance() {
        static SceneGraphSystem instance;
        return instance;
    }

    void SceneGraphSystem::initialize() {
        // Logger::getInstance().log(LogLevel::INFO, "SceneGraphSystem: Initializing Scene Graph System");
        clear();
    }

    void SceneGraphSystem::update(float deltaTime) {
        // Update transforms for entities marked as dirty
        if (!m_dirtyTransforms.empty()) {
            for (EntityID entityID : m_dirtyTransforms) {
                auto it = m_nodeMap.find(entityID);
                if (it != m_nodeMap.end()) {
                    updateTransformsFromNode(it->second);
                }
            }
            m_dirtyTransforms.clear();
        }
    }

    void SceneGraphSystem::clear() {
        Logger::getInstance().log(LogLevel::INFO, "SceneGraphSystem: Clearing scene graph");
        m_rootNodes.clear();
        m_nodeMap.clear();
        m_dirtyTransforms.clear();
    }

    // ============================================================================
    // Node Management
    // ============================================================================

    std::shared_ptr<SceneNode> SceneGraphSystem::addEntity(std::shared_ptr<Entity> entity, 
                                                          std::shared_ptr<SceneNode> parent) {
        if (!entity) {
            Logger::getInstance().log(LogLevel::WARNING, "SceneGraphSystem: Attempted to add null entity");
            return nullptr;
        }

        // Check if entity already exists in the scene graph
        auto existingNode = findNode(entity);
        if (existingNode) {
            Logger::getInstance().log(LogLevel::WARNING, 
                "SceneGraphSystem: Entity ID " + std::to_string(entity->getID()) + " already exists in scene graph");
            return existingNode;
        }

        // Create new scene node
        auto newNode = std::make_shared<SceneNode>(entity);
        m_nodeMap[entity->getID()] = newNode;

        // Set parent relationship
        if (parent) {
            addNodeToParent(newNode, parent);
        } else {
            m_rootNodes.push_back(newNode);
        }

        // Set up transform hierarchy if entities are transformable
        auto transformableChild = std::dynamic_pointer_cast<TransformableGameObject>(entity);
        if (transformableChild && parent && parent->entity) {
            auto transformableParent = std::dynamic_pointer_cast<TransformableGameObject>(parent->entity);
            if (transformableParent) {
                transformableChild->setParent(transformableParent.get());
            }
        }

        Logger::getInstance().log(LogLevel::INFO, 
            "SceneGraphSystem: Added entity ID " + std::to_string(entity->getID()) + " to scene graph");

        return newNode;
    }

    bool SceneGraphSystem::removeEntity(std::shared_ptr<Entity> entity) {
        if (!entity) {
            return false;
        }

        auto node = findNode(entity);
        if (!node) {
            return false;
        }

        return removeNode(node);
    }

    bool SceneGraphSystem::removeNode(std::shared_ptr<SceneNode> node) {
        if (!node) {
            return false;
        }

        // Remove from parent or root nodes
        removeNodeFromParent(node);

        // Reparent children to this node's parent
        for (auto& child : node->children) {
            if (node->parent) {
                addNodeToParent(child, node->parent);
            } else {
                child->parent = nullptr;
                m_rootNodes.push_back(child);
            }
        }

        // Remove from node map
        m_nodeMap.erase(node->entity->getID());

        // Remove from dirty transforms
        m_dirtyTransforms.erase(node->entity->getID());

        Logger::getInstance().log(LogLevel::INFO, 
            "SceneGraphSystem: Removed entity ID " + std::to_string(node->entity->getID()) + " from scene graph");

        return true;
    }

    std::shared_ptr<SceneNode> SceneGraphSystem::findNode(std::shared_ptr<Entity> entity) {
        if (!entity) {
            return nullptr;
        }
        return findNode(entity->getID());
    }

    std::shared_ptr<SceneNode> SceneGraphSystem::findNode(EntityID entityID) {
        auto it = m_nodeMap.find(entityID);
        return (it != m_nodeMap.end()) ? it->second : nullptr;
    }

    std::vector<std::shared_ptr<SceneNode>> SceneGraphSystem::findNodesByName(const std::string& name) {
        std::vector<std::shared_ptr<SceneNode>> results;
        
        for (const auto& pair : m_nodeMap) {
            auto transformableEntity = std::dynamic_pointer_cast<TransformableGameObject>(pair.second->entity);
            if (transformableEntity && transformableEntity->getName() == name) {
                results.push_back(pair.second);
            }
        }
        
        return results;
    }

    // ============================================================================
    // Hierarchy Management
    // ============================================================================

    bool SceneGraphSystem::setParent(std::shared_ptr<Entity> child, std::shared_ptr<Entity> parent) {
        if (!child) {
            return false;
        }

        auto childNode = findNode(child);
        if (!childNode) {
            Logger::getInstance().log(LogLevel::WARNING, 
                "SceneGraphSystem: Child entity not found in scene graph");
            return false;
        }

        std::shared_ptr<SceneNode> parentNode = nullptr;
        if (parent) {
            parentNode = findNode(parent);
            if (!parentNode) {
                Logger::getInstance().log(LogLevel::WARNING, 
                    "SceneGraphSystem: Parent entity not found in scene graph");
                return false;
            }
        }

        return setParent(childNode, parentNode);
    }

    bool SceneGraphSystem::setParent(std::shared_ptr<SceneNode> child, std::shared_ptr<SceneNode> parent) {
        if (!child) {
            return false;
        }

        // Check for circular dependency
        if (parent && isAncestorOf(child, parent)) {
            Logger::getInstance().log(LogLevel::ERROR, 
                "SceneGraphSystem: Cannot set parent: would create circular dependency");
            return false;
        }

        // Remove from current parent
        removeNodeFromParent(child);

        // Add to new parent
        if (parent) {
            addNodeToParent(child, parent);
        } else {
            child->parent = nullptr;
            m_rootNodes.push_back(child);
        }

        // Update transform hierarchy for transformable entities
        auto transformableChild = std::dynamic_pointer_cast<TransformableGameObject>(child->entity);
        if (transformableChild) {
            if (parent && parent->entity) {
                auto transformableParent = std::dynamic_pointer_cast<TransformableGameObject>(parent->entity);
                if (transformableParent) {
                    transformableChild->setParent(transformableParent.get());
                }
            } else {
                transformableChild->setParent(nullptr);
            }
        }

        // Mark transforms as dirty
        markTransformDirty(child->entity);

        return true;
    }

    std::vector<std::shared_ptr<SceneNode>> SceneGraphSystem::getAllNodes() const {
        std::vector<std::shared_ptr<SceneNode>> nodes;
        nodes.reserve(m_nodeMap.size());
        
        for (const auto& pair : m_nodeMap) {
            nodes.push_back(pair.second);
        }
        
        return nodes;
    }

    std::vector<std::shared_ptr<SceneNode>> SceneGraphSystem::getChildren(std::shared_ptr<Entity> entity) {
        auto node = findNode(entity);
        if (!node) {
            return {};
        }
        
        return node->children;
    }

    std::vector<std::shared_ptr<SceneNode>> SceneGraphSystem::getDescendants(std::shared_ptr<Entity> entity) {
        std::vector<std::shared_ptr<SceneNode>> descendants;
        auto node = findNode(entity);
        if (!node) {
            return descendants;
        }

        std::function<void(std::shared_ptr<SceneNode>)> collectDescendants = 
            [&](std::shared_ptr<SceneNode> current) {
                for (auto& child : current->children) {
                    descendants.push_back(child);
                    collectDescendants(child);
                }
            };

        collectDescendants(node);
        return descendants;
    }

    // ============================================================================
    // Traversal Methods
    // ============================================================================

    void SceneGraphSystem::traverseDepthFirst(std::function<void(std::shared_ptr<SceneNode>)> visitor, 
                                             bool rootsOnly) {
        if (rootsOnly) {
            for (auto& root : m_rootNodes) {
                traverseFromNode(root, visitor);
            }
        } else {
            for (const auto& pair : m_nodeMap) {
                visitor(pair.second);
            }
        }
    }

    void SceneGraphSystem::traverseBreadthFirst(std::function<void(std::shared_ptr<SceneNode>)> visitor, 
                                               bool rootsOnly) {
        if (rootsOnly) {
            std::queue<std::shared_ptr<SceneNode>> queue;
            
            // Add all root nodes to queue
            for (auto& root : m_rootNodes) {
                queue.push(root);
            }
            
            // Process queue
            while (!queue.empty()) {
                auto current = queue.front();
                queue.pop();
                
                visitor(current);
                
                // Add children to queue
                for (auto& child : current->children) {
                    queue.push(child);
                }
            }
        } else {
            // Simple iteration over all nodes
            for (const auto& pair : m_nodeMap) {
                visitor(pair.second);
            }
        }
    }

    void SceneGraphSystem::traverseFromNode(std::shared_ptr<SceneNode> startNode, 
                                           std::function<void(std::shared_ptr<SceneNode>)> visitor) {
        if (!startNode) {
            return;
        }
        
        visitor(startNode);
        
        for (auto& child : startNode->children) {
            traverseFromNode(child, visitor);
        }
    }

    // ============================================================================
    // Transform Updates
    // ============================================================================

    void SceneGraphSystem::updateTransforms() {
        for (auto& root : m_rootNodes) {
            updateTransformRecursive(root);
        }
        m_dirtyTransforms.clear();
    }

    void SceneGraphSystem::updateTransformsFromNode(std::shared_ptr<SceneNode> startNode) {
        if (!startNode) {
            return;
        }
        updateTransformRecursive(startNode);
    }

    void SceneGraphSystem::markTransformDirty(std::shared_ptr<Entity> entity) {
        if (entity) {
            m_dirtyTransforms.insert(entity->getID());
        }
    }

    // ============================================================================
    // Visibility and State Management
    // ============================================================================

    void SceneGraphSystem::setVisibility(std::shared_ptr<SceneNode> node, bool visible, bool recursive) {
        if (!node) {
            return;
        }
        
        node->isVisible = visible;
        
        if (recursive) {
            for (auto& child : node->children) {
                setVisibility(child, visible, true);
            }
        }
    }

    void SceneGraphSystem::setActive(std::shared_ptr<SceneNode> node, bool active, bool recursive) {
        if (!node) {
            return;
        }
        
        node->isActive = active;
        
        if (recursive) {
            for (auto& child : node->children) {
                setActive(child, active, true);
            }
        }
    }

    std::vector<std::shared_ptr<SceneNode>> SceneGraphSystem::getVisibleNodes() {
        std::vector<std::shared_ptr<SceneNode>> visibleNodes;
        
        for (auto& root : m_rootNodes) {
            collectVisibleNodes(root, visibleNodes);
        }
        
        return visibleNodes;
    }

    std::vector<std::shared_ptr<SceneNode>> SceneGraphSystem::getActiveNodes() {
        std::vector<std::shared_ptr<SceneNode>> activeNodes;
        
        for (auto& root : m_rootNodes) {
            collectActiveNodes(root, activeNodes);
        }
        
        return activeNodes;
    }

    // ============================================================================
    // Utility Methods
    // ============================================================================

    size_t SceneGraphSystem::getNodeDepth(std::shared_ptr<SceneNode> node) {
        if (!node) {
            return 0;
        }
        
        size_t depth = 0;
        auto current = node->parent;
        
        while (current) {
            depth++;
            current = current->parent;
        }
        
        return depth;
    }

    bool SceneGraphSystem::isAncestorOf(std::shared_ptr<SceneNode> ancestor, 
                                       std::shared_ptr<SceneNode> descendant) {
        if (!ancestor || !descendant) {
            return false;
        }
        
        auto current = descendant->parent;
        while (current) {
            if (current == ancestor) {
                return true;
            }
            current = current->parent;
        }
        
        return false;
    }

    std::vector<std::shared_ptr<SceneNode>> SceneGraphSystem::getPathToNode(std::shared_ptr<SceneNode> node) {
        std::vector<std::shared_ptr<SceneNode>> path;
        
        if (!node) {
            return path;
        }
        
        auto current = node;
        while (current) {
            path.insert(path.begin(), current);
            current = current->parent;
        }
        
        return path;
    }

    void SceneGraphSystem::printSceneGraph(bool detailed) {
        std::cout << "\n========== Scene Graph Structure ==========\n";
        std::cout << "Total Nodes: " << m_nodeMap.size() << "\n";
        std::cout << "Root Nodes: " << m_rootNodes.size() << "\n\n";
        
        for (auto& root : m_rootNodes) {
            printNodeRecursive(root, 0, detailed);
        }
        
        std::cout << "==========================================\n\n";
    }

    std::string SceneGraphSystem::getSceneGraphStats() {
        std::stringstream ss;
        ss << "Scene Graph Statistics:\n";
        ss << "  Total Nodes: " << m_nodeMap.size() << "\n";
        ss << "  Root Nodes: " << m_rootNodes.size() << "\n";
        ss << "  Maximum Depth: " << getMaxDepth() << "\n";
        
        size_t visibleCount = getVisibleNodes().size();
        size_t activeCount = getActiveNodes().size();
        
        ss << "  Visible Nodes: " << visibleCount << "\n";
        ss << "  Active Nodes: " << activeCount << "\n";
        ss << "  Dirty Transforms: " << m_dirtyTransforms.size() << "\n";
        
        return ss.str();
    }

    size_t SceneGraphSystem::getMaxDepth() {
        size_t maxDepth = 0;
        
        for (auto& root : m_rootNodes) {
            size_t depth = calculateMaxDepth(root);
            maxDepth = std::max(maxDepth, depth);
        }
        
        return maxDepth;
    }

    // ============================================================================
    // Helper Methods
    // ============================================================================

    void SceneGraphSystem::removeNodeFromParent(std::shared_ptr<SceneNode> node) {
        if (!node) {
            return;
        }
        
        if (node->parent) {
            // Remove from parent's children list
            auto& siblings = node->parent->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), node), siblings.end());
        } else {
            // Remove from root nodes
            m_rootNodes.erase(std::remove(m_rootNodes.begin(), m_rootNodes.end(), node), 
                             m_rootNodes.end());
        }
    }

    void SceneGraphSystem::addNodeToParent(std::shared_ptr<SceneNode> child, 
                                          std::shared_ptr<SceneNode> parent) {
        if (!child || !parent) {
            return;
        }
        
        child->parent = parent;
        parent->children.push_back(child);
    }

    void SceneGraphSystem::updateTransformRecursive(std::shared_ptr<SceneNode> node) {
        if (!node) {
            return;
        }
        
        // Update transformable entity transforms
        auto transformableEntity = std::dynamic_pointer_cast<TransformableGameObject>(node->entity);
        if (transformableEntity) {
            // Transform system handles hierarchical updates automatically through getWorldMatrix()
            // Just access the world matrix to ensure it's calculated
            transformableEntity->getTransform().getWorldMatrix();
        }
        
        // Recursively update children
        for (auto& child : node->children) {
            updateTransformRecursive(child);
        }
    }

    void SceneGraphSystem::collectVisibleNodes(std::shared_ptr<SceneNode> node, 
                                              std::vector<std::shared_ptr<SceneNode>>& result) {
        if (!node) {
            return;
        }
        
        if (node->isVisible) {
            result.push_back(node);
            
            // Only traverse children if this node is visible
            for (auto& child : node->children) {
                collectVisibleNodes(child, result);
            }
        }
    }

    void SceneGraphSystem::collectActiveNodes(std::shared_ptr<SceneNode> node, 
                                             std::vector<std::shared_ptr<SceneNode>>& result) {
        if (!node) {
            return;
        }
        
        if (node->isActive) {
            result.push_back(node);
            
            // Only traverse children if this node is active
            for (auto& child : node->children) {
                collectActiveNodes(child, result);
            }
        }
    }

    void SceneGraphSystem::printNodeRecursive(std::shared_ptr<SceneNode> node, int depth, bool detailed) {
        if (!node) {
            return;
        }
        
        // Print indentation
        for (int i = 0; i < depth; ++i) {
            std::cout << "  ";
        }
        
        // Print node info
        std::cout << "├─ Entity ID: " << node->entity->getID();
        
        auto transformableEntity = std::dynamic_pointer_cast<TransformableGameObject>(node->entity);
        if (transformableEntity) {
            std::cout << " (Name: " << transformableEntity->getName() << ")";
        }
        
        std::cout << " [Active: " << (node->isActive ? "Yes" : "No") 
                  << ", Visible: " << (node->isVisible ? "Yes" : "No") << "]";
        
        if (detailed && transformableEntity) {
            auto& transform = transformableEntity->getTransform();
            auto pos = transform.getPosition();
            std::cout << "\n";
            for (int i = 0; i <= depth; ++i) std::cout << "  ";
            std::cout << "   Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")";
        }
        
        std::cout << "\n";
        
        // Print children
        for (auto& child : node->children) {
            printNodeRecursive(child, depth + 1, detailed);
        }
    }

    size_t SceneGraphSystem::calculateMaxDepth(std::shared_ptr<SceneNode> node, size_t currentDepth) {
        if (!node) {
            return currentDepth;
        }
        
        size_t maxChildDepth = currentDepth;
        
        for (auto& child : node->children) {
            size_t childDepth = calculateMaxDepth(child, currentDepth + 1);
            maxChildDepth = std::max(maxChildDepth, childDepth);
        }
        
        return maxChildDepth;
    }

    // ============================================================================
    // Convenience Function
    // ============================================================================

    SceneGraphSystem& getSceneGraphSystem() {
        return SceneGraphSystem::getInstance();
    }

} // namespace IKore
