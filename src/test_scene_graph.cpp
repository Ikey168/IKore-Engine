#include "SceneGraphSystem.h"
#include "SceneManager.h"
#include "Logger.h"
#include <iostream>
#include <chrono>
#include <cassert>

using namespace IKore;

/**
 * @brief Comprehensive test suite for Scene Graph System
 */
class SceneGraphTests {
public:
    void runAllTests() {
        std::cout << "\n========== Scene Graph System Tests ==========\n\n";
        
        // Initialize systems
        Logger::getInstance().initialize();
        
        SceneGraphSystem::getInstance().initialize();
        SceneManager::getInstance().initialize();
        
        // Run test suites
        testBasicFunctionality();
        testHierarchyManagement();
        testTraversal();
        testVisibilityAndState();
        testSceneManager();
        testPerformance();
        testEdgeCases();
        
        std::cout << "\n========== All Tests Completed ==========\n\n";
    }
    
private:
    void testBasicFunctionality() {
        std::cout << "Testing Basic Functionality...\n";
        
        auto& sceneGraph = SceneGraphSystem::getInstance();
        sceneGraph.clear();
        
        // Test entity addition
        auto entity1 = std::make_shared<TransformableGameObject>("TestEntity1");
        
        auto node1 = sceneGraph.addEntity(entity1);
        assert(node1 != nullptr);
        assert(node1->entity == entity1);
        assert(sceneGraph.getNodeCount() == 1);
        
        // Test entity finding
        auto foundNode = sceneGraph.findNode(entity1);
        assert(foundNode == node1);
        
        auto foundByID = sceneGraph.findNode(entity1->getID());
        assert(foundByID == node1);
        
        // Test entity removal
        bool removed = sceneGraph.removeEntity(entity1);
        assert(removed);
        assert(sceneGraph.getNodeCount() == 0);
        
        std::cout << "✓ Basic functionality tests passed\n";
    }
    
    void testHierarchyManagement() {
        std::cout << "Testing Hierarchy Management...\n";
        
        auto& sceneGraph = SceneGraphSystem::getInstance();
        sceneGraph.clear();
        
        // Create parent-child hierarchy
        auto parent = std::make_shared<TransformableGameObject>("Parent");
        auto parentNode = sceneGraph.addEntity(parent);
        
        auto child1 = std::make_shared<TransformableGameObject>("Child1");
        auto child1Node = sceneGraph.addEntity(child1, parentNode);
        
        auto child2 = std::make_shared<TransformableGameObject>("Child2");
        auto child2Node = sceneGraph.addEntity(child2, parentNode);
        
        // Test hierarchy structure
        assert(child1Node->parent == parentNode);
        assert(child2Node->parent == parentNode);
        assert(parentNode->children.size() == 2);
        assert(sceneGraph.getRootNodeCount() == 1);
        
        // Test reparenting
        bool reparented = sceneGraph.setParent(child2Node, child1Node);
        assert(reparented);
        assert(child2Node->parent == child1Node);
        assert(child1Node->children.size() == 1);
        assert(parentNode->children.size() == 1);
        
        // Test circular dependency prevention
        bool circular = sceneGraph.setParent(parentNode, child2Node);
        assert(!circular); // Should fail
        
        // Test transform hierarchy
        parent->getTransform().setPosition(glm::vec3(10.0f, 0.0f, 0.0f));
        child1->getTransform().setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        child2->getTransform().setPosition(glm::vec3(2.0f, 0.0f, 0.0f));
        
        sceneGraph.updateTransforms();
        
        // Child2 should be at parent(10) + child1(5) + child2(2) = 17 in world space
        auto worldPos = child2->getTransform().getWorldPosition();
        assert(std::abs(worldPos.x - 17.0f) < 0.001f);
        
        std::cout << "✓ Hierarchy management tests passed\n";
    }
    
    void testTraversal() {
        std::cout << "Testing Traversal Methods...\n";
        
        auto& sceneGraph = SceneGraphSystem::getInstance();
        sceneGraph.clear();
        
        // Create a complex hierarchy
        auto root = std::make_shared<TransformableGameObject>("Root");
        auto rootNode = sceneGraph.addEntity(root);
    // Moved to src/tests/test_scene_graph.cpp
        
        std::vector<std::shared_ptr<SceneNode>> allNodes;
        allNodes.push_back(rootNode);
        
        // Create multiple levels
        for (int i = 0; i < 3; ++i) {
            auto child = std::make_shared<TransformableGameObject>("Level1_" + std::to_string(i));
            auto childNode = sceneGraph.addEntity(child, rootNode);
            allNodes.push_back(childNode);
            
            for (int j = 0; j < 2; ++j) {
                auto grandchild = std::make_shared<TransformableGameObject>("Level2_" + std::to_string(i) + "_" + std::to_string(j));
                auto grandchildNode = sceneGraph.addEntity(grandchild, childNode);
                allNodes.push_back(grandchildNode);
            }
        }
        
        // Test depth-first traversal
        std::vector<std::shared_ptr<SceneNode>> visitedDepthFirst;
        sceneGraph.traverseDepthFirst([&](std::shared_ptr<SceneNode> node) {
            visitedDepthFirst.push_back(node);
        });
        
        assert(visitedDepthFirst.size() == allNodes.size());
        assert(visitedDepthFirst[0] == rootNode); // Root should be first
        
        // Test breadth-first traversal
        std::vector<std::shared_ptr<SceneNode>> visitedBreadthFirst;
        sceneGraph.traverseBreadthFirst([&](std::shared_ptr<SceneNode> node) {
            visitedBreadthFirst.push_back(node);
        });
        
        assert(visitedBreadthFirst.size() == allNodes.size());
        assert(visitedBreadthFirst[0] == rootNode); // Root should be first
        
        // Test traversal from specific node
        std::vector<std::shared_ptr<SceneNode>> visitedFromNode;
        sceneGraph.traverseFromNode(allNodes[1], [&](std::shared_ptr<SceneNode> node) {
            visitedFromNode.push_back(node);
        });
        
        assert(visitedFromNode.size() == 3); // Node + 2 children
        
        std::cout << "✓ Traversal tests passed\n";
    }
    
    void testVisibilityAndState() {
        std::cout << "Testing Visibility and State Management...\n";
        
        auto& sceneGraph = SceneGraphSystem::getInstance();
        sceneGraph.clear();
        
        auto parent = std::make_shared<TransformableGameObject>("Parent");
        auto parentNode = sceneGraph.addEntity(parent);
        
        auto child = std::make_shared<TransformableGameObject>("Child");
        auto childNode = sceneGraph.addEntity(child, parentNode);
        
        // Test visibility
        assert(parentNode->isVisible == true);
        assert(childNode->isVisible == true);
        
        sceneGraph.setVisibility(parentNode, false, false);
        assert(parentNode->isVisible == false);
        assert(childNode->isVisible == true); // Child should remain visible
        
        sceneGraph.setVisibility(parentNode, false, true);
        assert(parentNode->isVisible == false);
        assert(childNode->isVisible == false); // Child should be affected by recursive
        
        // Test active state
        sceneGraph.setActive(parentNode, true, true);
        assert(parentNode->isActive == true);
        assert(childNode->isActive == true);
        
        sceneGraph.setActive(parentNode, false, true);
        assert(parentNode->isActive == false);
        assert(childNode->isActive == false);
        
        // Test filtered queries
        sceneGraph.setVisibility(parentNode, true, true);
        sceneGraph.setActive(parentNode, true, true);
        
        auto visibleNodes = sceneGraph.getVisibleNodes();
        auto activeNodes = sceneGraph.getActiveNodes();
        
        assert(visibleNodes.size() == 2);
        assert(activeNodes.size() == 2);
        
        std::cout << "✓ Visibility and state tests passed\n";
    }
    
    void testSceneManager() {
        std::cout << "Testing Scene Manager...\n";
        
        auto& sceneManager = SceneManager::getInstance();
        sceneManager.clearScene();
        
        // Test entity creation
        auto gameObject = sceneManager.createGameObject("TestObject");
        assert(gameObject != nullptr);
        assert(gameObject->getName() == "TestObject");
        
        auto light = sceneManager.createLight("TestLight");
        assert(light != nullptr);
        assert(light->getName() == "TestLight");
        
        auto camera = sceneManager.createCamera("TestCamera");
        assert(camera != nullptr);
        assert(camera->getName() == "TestCamera");
        
        // Test finding
        auto foundObject = sceneManager.findFirstEntityByName("TestObject");
        assert(foundObject == gameObject);
        
        auto allGameObjects = sceneManager.getAllGameObjects();
        assert(allGameObjects.size() == 1);
        assert(allGameObjects[0] == gameObject);
        
        auto allLights = sceneManager.getAllLights();
        assert(allLights.size() == 1);
        assert(allLights[0] == light);
        
        auto allCameras = sceneManager.getAllCameras();
        assert(allCameras.size() == 1);
        assert(allCameras[0] == camera);
        
        // Test hierarchy operations
        bool parentSet = sceneManager.setParent(light, gameObject);
        assert(parentSet);
        
        auto children = sceneManager.getChildren(gameObject);
        assert(children.size() == 1);
        assert(children[0] == light);
        
        auto parent = sceneManager.getParent(light);
        assert(parent == gameObject);
        
        // Test scene presets
        sceneManager.clearScene();
        auto basicScene = sceneManager.createBasicScene();
        assert(basicScene.size() == 3); // Camera, Light, SceneRoot
        
        auto hierarchyDemo = sceneManager.createHierarchyDemo();
        assert(hierarchyDemo != nullptr);
        assert(hierarchyDemo->getName() == "HierarchyParent");
        
        // Test scene validation
        bool isValid = sceneManager.validateSceneIntegrity();
        assert(isValid);
        
        std::cout << "✓ Scene Manager tests passed\n";
    }
    
    void testPerformance() {
        std::cout << "Testing Performance...\n";
        
        auto& sceneGraph = SceneGraphSystem::getInstance();
        sceneGraph.clear();
        
        const size_t entityCount = 1000;
        std::vector<std::shared_ptr<TransformableGameObject>> entities;
        
        // Create many entities
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < entityCount; ++i) {
            auto entity = std::make_shared<TransformableGameObject>("Entity_" + std::to_string(i));
            entities.push_back(entity);
            sceneGraph.addEntity(entity);
        }
        
        auto addTime = std::chrono::high_resolution_clock::now();
        
        // Test traversal performance
        size_t visitCount = 0;
        sceneGraph.traverseDepthFirst([&](std::shared_ptr<SceneNode> node) {
            visitCount++;
        });
        
        auto traversalTime = std::chrono::high_resolution_clock::now();
        
        // Test transform update performance
        for (auto& entity : entities) {
            entity->getTransform().setPosition(glm::vec3(1.0f, 0.0f, 0.0f));
            sceneGraph.markTransformDirty(entity);
        }
        
        sceneGraph.updateTransforms();
        
        auto updateTime = std::chrono::high_resolution_clock::now();
        
        // Calculate timings
        auto addDuration = std::chrono::duration_cast<std::chrono::microseconds>(addTime - start);
        auto traversalDuration = std::chrono::duration_cast<std::chrono::microseconds>(traversalTime - addTime);
        auto updateDuration = std::chrono::duration_cast<std::chrono::microseconds>(updateTime - traversalTime);
        
        std::cout << "  Created " << entityCount << " entities in " << addDuration.count() << " μs\n";
        std::cout << "  Traversed " << visitCount << " nodes in " << traversalDuration.count() << " μs\n";
        std::cout << "  Updated " << entityCount << " transforms in " << updateDuration.count() << " μs\n";
        
        assert(visitCount == entityCount);
        assert(sceneGraph.getNodeCount() == entityCount);
        
        std::cout << "✓ Performance tests passed\n";
    }
    
    void testEdgeCases() {
        std::cout << "Testing Edge Cases...\n";
        
        auto& sceneGraph = SceneGraphSystem::getInstance();
        sceneGraph.clear();
        
        // Test null entity handling
        auto nullNode = sceneGraph.addEntity(nullptr);
        assert(nullNode == nullptr);
        
        bool nullRemoved = sceneGraph.removeEntity(nullptr);
        assert(!nullRemoved);
        
        auto nullFound = sceneGraph.findNode(nullptr);
        assert(nullFound == nullptr);
        
        // Test duplicate entity addition
        auto entity = std::make_shared<TransformableGameObject>("TestEntity");
        auto node1 = sceneGraph.addEntity(entity);
        auto node2 = sceneGraph.addEntity(entity); // Should return existing node
        assert(node1 == node2);
        assert(sceneGraph.getNodeCount() == 1);
        
        // Test removing non-existent entity
        auto otherEntity = std::make_shared<TransformableGameObject>("OtherEntity");
        bool removedNonExistent = sceneGraph.removeEntity(otherEntity);
        assert(!removedNonExistent);
        
        // Test deep hierarchy
        auto root = sceneGraph.addEntity(std::make_shared<TransformableGameObject>("Root"));
        auto current = root;
        
        const int maxDepth = 100;
        for (int i = 0; i < maxDepth; ++i) {
            auto newEntity = std::make_shared<TransformableGameObject>("Deep_" + std::to_string(i));
            auto newNode = sceneGraph.addEntity(newEntity, current);
            current = newNode;
        }
        
        assert(sceneGraph.getMaxDepth() == maxDepth);
        assert(sceneGraph.getNodeDepth(current) == maxDepth);
        
        // Test path finding
        auto path = sceneGraph.getPathToNode(current);
        assert(path.size() == maxDepth + 1); // Including root
        assert(path[0] == root);
        assert(path.back() == current);
        
        std::cout << "✓ Edge case tests passed\n";
    }
};

int main() {
    try {
        SceneGraphTests tests;
        tests.runAllTests();
        
        std::cout << "🎉 All Scene Graph System tests passed successfully!\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
