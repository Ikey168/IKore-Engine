#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <cassert>

#include "core/components/AIComponent.h"
#include "core/AISystem.h"
#include "core/Entity.h"
#include "core/components/TransformComponent.h"

using namespace IKore;

/**
 * @brief Test program for AI Component and AI System
 * 
 * This program tests:
 * - AI Component creation and behavior configuration
 * - AI System registration and management
 * - Basic AI behaviors (idle, patrol, chase, flee)
 * - Event-driven AI interactions
 * - Performance with multiple AI entities
 */

/**
 * @brief Test basic AI component creation and configuration
 */
void testAIComponentCreation() {
    std::cout << "\n=== Testing AI Component Creation ===" << std::endl;
    
    // Create an entity with AI component
    auto entity = std::make_shared<Entity>();
    auto aiComponent = entity->addComponent<AIComponent>();
    auto transform = entity->addComponent<TransformComponent>();
    
    assert(aiComponent != nullptr);
    assert(transform != nullptr);
    
    // Test initial state
    assert(aiComponent->getState() == AIState::INACTIVE);
    assert(aiComponent->getActiveBehavior().empty());
    assert(aiComponent->getBehaviorNames().empty());
    
    std::cout << "✅ AI Component created successfully" << std::endl;
    
    // Add idle behavior
    AIBehaviorConfig idleConfig;
    idleConfig.type = AIBehaviorType::IDLE;
    idleConfig.priority = 1.0f;
    aiComponent->addBehavior("idle", idleConfig);
    
    assert(aiComponent->hasBehavior("idle"));
    assert(aiComponent->getActiveBehavior() == "idle");
    assert(aiComponent->getState() == AIState::ACTIVE);
    
    std::cout << "✅ Idle behavior configured and activated" << std::endl;
    
    // Add patrol behavior
    AIBehaviorConfig patrolConfig;
    patrolConfig.type = AIBehaviorType::PATROL;
    patrolConfig.priority = 2.0f;
    patrolConfig.speed = 5.0f;
    patrolConfig.waypoints = {
        glm::vec3(0, 0, 0),
        glm::vec3(10, 0, 0),
        glm::vec3(10, 0, 10),
        glm::vec3(0, 0, 10)
    };
    aiComponent->addBehavior("patrol", patrolConfig);
    
    assert(aiComponent->hasBehavior("patrol"));
    std::cout << "✅ Patrol behavior added successfully" << std::endl;
    
    // Test behavior switching
    aiComponent->setActiveBehavior("patrol");
    assert(aiComponent->getActiveBehavior() == "patrol");
    
    const auto* config = aiComponent->getCurrentBehaviorConfig();
    assert(config != nullptr);
    assert(config->type == AIBehaviorType::PATROL);
    assert(config->speed == 5.0f);
    assert(config->waypoints.size() == 4);
    
    std::cout << "✅ Behavior switching working correctly" << std::endl;
}

/**
 * @brief Test AI System registration and management
 */
void testAISystemManagement() {
    std::cout << "\n=== Testing AI System Management ===" << std::endl;
    
    auto& aiSystem = AISystem::getInstance();
    
    // Test initial state
    assert(aiSystem.getRegisteredAICount() == 0);
    assert(aiSystem.getActiveAICount() == 0);
    
    std::cout << "✅ AI System initialized correctly" << std::endl;
    
    // Create AI entities
    std::vector<std::shared_ptr<Entity>> entities;
    for (int i = 0; i < 5; ++i) {
        auto entity = std::make_shared<Entity>();
        auto aiComponent = entity->addComponent<AIComponent>();
        auto transform = entity->addComponent<TransformComponent>();
        
        // Add idle behavior
        AIBehaviorConfig idleConfig;
        idleConfig.type = AIBehaviorType::IDLE;
        aiComponent->addBehavior("idle", idleConfig);
        
        // Register with AI system
        aiSystem.registerAIEntity(entity);
        entities.push_back(entity);
    }
    
    assert(aiSystem.getRegisteredAICount() == 5);
    assert(aiSystem.getActiveAICount() == 5);
    
    std::cout << "✅ Successfully registered 5 AI entities" << std::endl;
    
    // Test update
    aiSystem.updateAll(0.016f); // 60 FPS
    std::cout << "✅ AI System update completed without errors" << std::endl;
    
    // Test unregistration
    aiSystem.unregisterAIEntity(entities[0]);
    assert(aiSystem.getRegisteredAICount() == 4);
    
    std::cout << "✅ AI entity unregistration working correctly" << std::endl;
}

/**
 * @brief Test AI behaviors
 */
void testAIBehaviors() {
    std::cout << "\n=== Testing AI Behaviors ===" << std::endl;
    
    auto entity = std::make_shared<Entity>();
    auto aiComponent = entity->addComponent<AIComponent>();
    auto transform = entity->addComponent<TransformComponent>();
    
    transform->setPosition(glm::vec3(0, 0, 0));
    
    // Test Patrol Behavior
    AIBehaviorConfig patrolConfig;
    patrolConfig.type = AIBehaviorType::PATROL;
    patrolConfig.speed = 10.0f;
    patrolConfig.waypoints = {
        glm::vec3(0, 0, 0),
        glm::vec3(5, 0, 0),
        glm::vec3(5, 0, 5)
    };
    aiComponent->addBehavior("patrol", patrolConfig);
    aiComponent->setActiveBehavior("patrol");
    
    // Simulate movement
    glm::vec3 initialPos = transform->getPosition();
    for (int i = 0; i < 10; ++i) {
        aiComponent->update(0.1f); // 10 FPS for slower movement
    }
    
    glm::vec3 finalPos = transform->getPosition();
    float distance = glm::length(finalPos - initialPos);
    
    std::cout << "Patrol movement distance: " << distance << std::endl;
    std::cout << "✅ Patrol behavior execution completed" << std::endl;
    
    // Test Chase Behavior
    auto targetEntity = std::make_shared<Entity>();
    auto targetTransform = targetEntity->addComponent<TransformComponent>();
    targetTransform->setPosition(glm::vec3(10, 0, 0));
    
    AIBehaviorConfig chaseConfig;
    chaseConfig.type = AIBehaviorType::CHASE;
    chaseConfig.speed = 5.0f;
    chaseConfig.detectionRange = 20.0f;
    aiComponent->addBehavior("chase", chaseConfig);
    aiComponent->setTarget(targetEntity);
    aiComponent->setActiveBehavior("chase");
    
    glm::vec3 chaseStartPos = transform->getPosition();
    for (int i = 0; i < 5; ++i) {
        aiComponent->update(0.1f);
    }
    
    glm::vec3 chaseEndPos = transform->getPosition();
    float chaseDistance = glm::length(chaseEndPos - chaseStartPos);
    
    std::cout << "Chase movement distance: " << chaseDistance << std::endl;
    std::cout << "✅ Chase behavior execution completed" << std::endl;
}

/**
 * @brief Test event-driven AI interactions
 */
void testAIEvents() {
    std::cout << "\n=== Testing AI Events ===" << std::endl;
    
    auto entity = std::make_shared<Entity>();
    auto aiComponent = entity->addComponent<AIComponent>();
    auto transform = entity->addComponent<TransformComponent>();
    
    // Add behaviors
    AIBehaviorConfig idleConfig;
    idleConfig.type = AIBehaviorType::IDLE;
    idleConfig.priority = 1.0f;
    aiComponent->addBehavior("idle", idleConfig);
    
    AIBehaviorConfig chaseConfig;
    chaseConfig.type = AIBehaviorType::CHASE;
    chaseConfig.priority = 5.0f;
    aiComponent->addBehavior("chase", chaseConfig);
    
    aiComponent->setActiveBehavior("idle");
    assert(aiComponent->getActiveBehavior() == "idle");
    
    // Add event listener
    bool eventProcessed = false;
    aiComponent->addEventListener("enemy_spotted", 
        [&eventProcessed](const AIEvent& event) -> std::string {
            eventProcessed = true;
            return "chase"; // Switch to chase behavior
        });
    
    // Create and process event
    AIEvent spotEvent;
    spotEvent.type = "enemy_spotted";
    spotEvent.position = glm::vec3(5, 0, 0);
    spotEvent.intensity = 1.0f;
    spotEvent.timestamp = std::chrono::steady_clock::now();
    
    aiComponent->processEvent(spotEvent);
    
    assert(eventProcessed);
    assert(aiComponent->getActiveBehavior() == "chase");
    
    std::cout << "✅ Event-driven behavior switching working correctly" << std::endl;
}

/**
 * @brief Test AI system performance with many entities
 */
void testAIPerformance() {
    std::cout << "\n=== Testing AI Performance ===" << std::endl;
    
    auto& aiSystem = AISystem::getInstance();
    
    // Clear any existing entities
    std::vector<std::shared_ptr<Entity>> entities;
    
    // Create many AI entities
    const int numEntities = 100;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numEntities; ++i) {
        auto entity = std::make_shared<Entity>();
        auto aiComponent = entity->addComponent<AIComponent>();
        auto transform = entity->addComponent<TransformComponent>();
        
        transform->setPosition(glm::vec3(i * 2.0f, 0, 0));
        
        // Add patrol behavior
        AIBehaviorConfig patrolConfig;
        patrolConfig.type = AIBehaviorType::PATROL;
        patrolConfig.speed = 2.0f;
        patrolConfig.waypoints = {
            glm::vec3(i * 2.0f, 0, 0),
            glm::vec3(i * 2.0f + 5, 0, 0),
            glm::vec3(i * 2.0f + 5, 0, 5),
            glm::vec3(i * 2.0f, 0, 5)
        };
        aiComponent->addBehavior("patrol", patrolConfig);
        
        aiSystem.registerAIEntity(entity);
        entities.push_back(entity);
    }
    
    auto setupEnd = std::chrono::high_resolution_clock::now();
    
    // Test update performance
    for (int frame = 0; frame < 100; ++frame) {
        aiSystem.updateAll(0.016f); // 60 FPS
    }
    
    auto updateEnd = std::chrono::high_resolution_clock::now();
    
    auto setupTime = std::chrono::duration_cast<std::chrono::microseconds>(setupEnd - start);
    auto updateTime = std::chrono::duration_cast<std::chrono::microseconds>(updateEnd - setupEnd);
    
    std::cout << "✅ Performance test completed" << std::endl;
    std::cout << "   Setup time for " << numEntities << " AI entities: " << setupTime.count() << " μs" << std::endl;
    std::cout << "   Update time for 100 frames: " << updateTime.count() << " μs" << std::endl;
    std::cout << "   Average update time per frame: " << (updateTime.count() / 100.0) << " μs" << std::endl;
    std::cout << "   Registered AI count: " << aiSystem.getRegisteredAICount() << std::endl;
    std::cout << "   Active AI count: " << aiSystem.getActiveAICount() << std::endl;
}

int main() {
    std::cout << "🤖 AI Component and System Test Suite 🤖" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    try {
        testAIComponentCreation();
        testAISystemManagement();
        testAIBehaviors();
        testAIEvents();
        testAIPerformance();
        
        std::cout << "\n🎉 All AI tests passed! AI Component system is working correctly." << std::endl;
        
        // Print final statistics
        auto& aiSystem = AISystem::getInstance();
        aiSystem.printStatistics();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
