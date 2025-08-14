#include "SceneGraphSystem.h"
#include "SceneManager.h"
#include "Logger.h"
#include <iostream>

using namespace IKore;

/**
 * @brief Scene Graph System integration example
 * 
 * This example demonstrates how to integrate the Scene Graph System
 * into the main application loop and use it for scene management.
 */
void demonstrateSceneGraphIntegration() {
    std::cout << "\n========== Scene Graph System Integration Demo ==========\n\n";
    
    // Initialize systems
    Logger::getInstance().initialize();
    
    SceneGraphSystem::getInstance().initialize();
    SceneManager::getInstance().initialize();
    
    auto& sceneManager = SceneManager::getInstance();
    
    // Create a basic scene
    std::cout << "Creating basic scene...\n";
    auto basicEntities = sceneManager.createBasicScene();
    
    // Create a more complex hierarchy
    std::cout << "Creating complex hierarchy...\n";
    
    // Create a vehicle hierarchy
    auto vehicle = sceneManager.createGameObject("Vehicle");
    vehicle->getTransform().setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    
    // Add wheels as children
    auto wheelFL = sceneManager.createGameObject("WheelFrontLeft", vehicle);
    wheelFL->getTransform().setPosition(glm::vec3(-1.0f, -0.5f, 1.0f));
    
    auto wheelFR = sceneManager.createGameObject("WheelFrontRight", vehicle);
    wheelFR->getTransform().setPosition(glm::vec3(1.0f, -0.5f, 1.0f));
    
    auto wheelRL = sceneManager.createGameObject("WheelRearLeft", vehicle);
    wheelRL->getTransform().setPosition(glm::vec3(-1.0f, -0.5f, -1.0f));
    
    auto wheelRR = sceneManager.createGameObject("WheelRearRight", vehicle);
    wheelRR->getTransform().setPosition(glm::vec3(1.0f, -0.5f, -1.0f));
    
    // Add a driver as child of vehicle
    auto driver = sceneManager.createGameObject("Driver", vehicle);
    driver->getTransform().setPosition(glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Create a building hierarchy
    auto building = sceneManager.createGameObject("Building");
    building->getTransform().setPosition(glm::vec3(10.0f, 0.0f, 0.0f));
    
    // Add floors
    for (int floor = 0; floor < 3; ++floor) {
        auto floorEntity = sceneManager.createGameObject("Floor_" + std::to_string(floor), building);
        floorEntity->getTransform().setPosition(glm::vec3(0.0f, floor * 3.0f, 0.0f));
        
        // Add rooms to each floor
        for (int room = 0; room < 4; ++room) {
            auto roomEntity = sceneManager.createGameObject("Room_" + std::to_string(room), floorEntity);
            roomEntity->getTransform().setPosition(glm::vec3(
                (room % 2) * 5.0f - 2.5f, 
                0.0f, 
                (room / 2) * 5.0f - 2.5f
            ));
            
            // Add furniture to first room of first floor
            if (floor == 0 && room == 0) {
                auto table = sceneManager.createGameObject("Table", roomEntity);
                table->getTransform().setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
                
                auto chair1 = sceneManager.createGameObject("Chair1", roomEntity);
                chair1->getTransform().setPosition(glm::vec3(1.0f, 0.0f, 0.0f));
                
                auto chair2 = sceneManager.createGameObject("Chair2", roomEntity);
                chair2->getTransform().setPosition(glm::vec3(-1.0f, 0.0f, 0.0f));
            }
        }
    }
    
    // Print current scene structure
    std::cout << "\nCurrent scene structure:\n";
    sceneManager.printScene(false);
    
    // Demonstrate scene queries
    std::cout << "\nScene queries:\n";
    std::cout << "Total entities: " << sceneManager.getEntityCount() << "\n";
    std::cout << "Max hierarchy depth: " << sceneManager.getMaxDepth() << "\n";
    
    auto allGameObjects = sceneManager.getAllGameObjects();
    std::cout << "Game objects: " << allGameObjects.size() << "\n";
    
    auto allLights = sceneManager.getAllLights();
    std::cout << "Lights: " << allLights.size() << "\n";
    
    auto allCameras = sceneManager.getAllCameras();
    std::cout << "Cameras: " << allCameras.size() << "\n";
    
    // Demonstrate hierarchy manipulation
    std::cout << "\nMoving vehicle and updating transforms...\n";
    vehicle->getTransform().setPosition(glm::vec3(5.0f, 0.0f, 5.0f));
    vehicle->getTransform().setRotation(glm::vec3(0.0f, 45.0f, 0.0f));
    
    sceneManager.updateTransforms();
    
    // Show world positions after transform
    std::cout << "Vehicle world position: " 
              << vehicle->getTransform().getWorldPosition().x << ", "
              << vehicle->getTransform().getWorldPosition().y << ", "
              << vehicle->getTransform().getWorldPosition().z << "\n";
    
    std::cout << "Driver world position: " 
              << driver->getTransform().getWorldPosition().x << ", "
              << driver->getTransform().getWorldPosition().y << ", "
              << driver->getTransform().getWorldPosition().z << "\n";
    
    // Demonstrate visibility management
    std::cout << "\nTesting visibility management...\n";
    sceneManager.setVisibility(building, false, true); // Hide entire building
    
    auto visibleEntities = sceneManager.getVisibleEntities();
    std::cout << "Visible entities after hiding building: " << visibleEntities.size() << "\n";
    
    sceneManager.setVisibility(building, true, true); // Show building again
    visibleEntities = sceneManager.getVisibleEntities();
    std::cout << "Visible entities after showing building: " << visibleEntities.size() << "\n";
    
    // Demonstrate traversal
    std::cout << "\nTraversing all game objects:\n";
    sceneManager.forEachGameObject([](std::shared_ptr<TransformableGameObject> obj) {
        auto pos = obj->getTransform().getWorldPosition();
        std::cout << "  " << obj->getName() << " at (" 
                  << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
    });
    
    // Demonstrate finding entities
    std::cout << "\nFinding specific entities:\n";
    auto foundVehicle = sceneManager.findFirstEntityByName("Vehicle");
    if (foundVehicle) {
        std::cout << "Found vehicle with ID: " << foundVehicle->getID() << "\n";
        
        auto vehicleChildren = sceneManager.getChildren(foundVehicle);
        std::cout << "Vehicle has " << vehicleChildren.size() << " children\n";
        
        auto vehicleDescendants = sceneManager.getDescendants(foundVehicle);
        std::cout << "Vehicle has " << vehicleDescendants.size() << " total descendants\n";
    }
    
    // Demonstrate scene statistics
    std::cout << "\nScene statistics:\n";
    std::cout << sceneManager.getSceneStats();
    
    // Validate scene integrity
    std::cout << "\nValidating scene integrity...\n";
    bool isValid = sceneManager.validateSceneIntegrity();
    std::cout << "Scene integrity: " << (isValid ? "VALID" : "INVALID") << "\n";
    
    // Cleanup
    std::cout << "\nCleaning up scene...\n";
    sceneManager.clearScene();
    std::cout << "Final entity count: " << sceneManager.getEntityCount() << "\n";
    
    std::cout << "\n========== Integration Demo Complete ==========\n\n";
}

int main() {
    try {
        demonstrateSceneGraphIntegration();
        std::cout << "🎉 Scene Graph System integration demo completed successfully!\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "❌ Demo failed with exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "❌ Demo failed with unknown exception" << std::endl;
        return 1;
    }
}
