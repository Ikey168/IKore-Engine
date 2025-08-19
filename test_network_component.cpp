#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <cstring> // For memcpy
#include "core/Entity.h"
#include "core/NetworkSystem.h"
#include "core/components/NetworkComponent.h"
#include "core/components/TransformComponent.h"
#include "core/Logger.h"

using namespace IKore;

// Test data for serialization
struct PlayerData {
    glm::vec3 position;
    float health;
    int score;
};

// Serialize player data to binary
std::vector<uint8_t> serializePlayerData(const PlayerData& data) {
    std::vector<uint8_t> result;
    
    // Position
    const uint8_t* posBytes = reinterpret_cast<const uint8_t*>(&data.position);
    result.insert(result.end(), posBytes, posBytes + sizeof(glm::vec3));
    
    // Health
    const uint8_t* healthBytes = reinterpret_cast<const uint8_t*>(&data.health);
    result.insert(result.end(), healthBytes, healthBytes + sizeof(float));
    
    // Score
    const uint8_t* scoreBytes = reinterpret_cast<const uint8_t*>(&data.score);
    result.insert(result.end(), scoreBytes, scoreBytes + sizeof(int));
    
    return result;
}

// Deserialize player data from binary
PlayerData deserializePlayerData(const std::vector<uint8_t>& data) {
    PlayerData result;
    
    // Position
    memcpy(&result.position, data.data(), sizeof(glm::vec3));
    
    // Health
    memcpy(&result.health, data.data() + sizeof(glm::vec3), sizeof(float));
    
    // Score
    memcpy(&result.score, data.data() + sizeof(glm::vec3) + sizeof(float), sizeof(int));
    
    return result;
}

// Print player data
void printPlayerData(const PlayerData& data) {
    std::cout << "Position: [" << data.position.x << ", " << data.position.y << ", " << data.position.z << "]" << std::endl;
    std::cout << "Health: " << data.health << std::endl;
    std::cout << "Score: " << data.score << std::endl;
}

// Initialize logger
void initLogger() {
    Logger::getInstance().initialize("network_component_test.log");
}

int main() {
    // Initialize logging
    initLogger();
    
    LOG_INFO("Starting Network Component Test");
    
    // Initialize the network system as a server
    auto& networkSystem = NetworkSystem::getInstance();
    if (!networkSystem.initialize(true, 7777)) {
        LOG_ERROR("Failed to initialize network system");
        return 1;
    }
    
    // Create a player entity
    auto player = std::make_shared<Entity>();
    player->addComponent<TransformComponent>();
    
    // Register the entity with the network system
    if (!networkSystem.registerNetworkedEntity(player, "player1")) {
        LOG_ERROR("Failed to register player entity");
        return 1;
    }
    
    // Get the network component
    auto netComp = player->getComponent<NetworkComponent>();
    if (!netComp) {
        LOG_ERROR("Failed to get network component");
        return 1;
    }
    
    // Register some properties for synchronization
    netComp->registerProperty("position", SyncMode::PREDICTIVE, 10.0f, 20.0f, true);
    netComp->registerProperty("health", SyncMode::AUTHORITATIVE, 5.0f, 5.0f, true);
    netComp->registerProperty("score", SyncMode::AUTHORITATIVE, 3.0f, 2.0f, true);
    
    // Set up a callback for the position property
    netComp->setPropertyUpdateCallback("position", [](const std::vector<uint8_t>& data) {
        glm::vec3 position;
        memcpy(&position, data.data(), sizeof(glm::vec3));
        std::cout << "Received position update: [" << position.x << ", " << position.y << ", " << position.z << "]" << std::endl;
    });
    
    // Simulate game loop
    PlayerData playerData = {
        glm::vec3(0.0f, 0.0f, 0.0f), // position
        100.0f,                      // health
        0                            // score
    };
    
    for (int i = 0; i < 10; i++) {
        // Update player data
        playerData.position.x += 1.0f;
        playerData.position.x += 1.0f;
        playerData.position.y += 0.5f;
        playerData.score += 10;
        
        if (i == 5) {
            playerData.health -= 25.0f;
        }
        
        // Serialize and send the updates
        std::vector<uint8_t> positionData(reinterpret_cast<uint8_t*>(&playerData.position), 
                                         reinterpret_cast<uint8_t*>(&playerData.position) + sizeof(glm::vec3));
        
        std::vector<uint8_t> healthData(reinterpret_cast<uint8_t*>(&playerData.health), 
                                       reinterpret_cast<uint8_t*>(&playerData.health) + sizeof(float));
        
        std::vector<uint8_t> scoreData(reinterpret_cast<uint8_t*>(&playerData.score), 
                                      reinterpret_cast<uint8_t*>(&playerData.score) + sizeof(int));
        
        // Send the updates
        netComp->sendPropertyUpdate("position", positionData);
        netComp->sendPropertyUpdate("health", healthData);
        netComp->sendPropertyUpdate("score", scoreData);
        
        // Print current state
        std::cout << "\n--- Frame " << i+1 << " ---" << std::endl;
        printPlayerData(playerData);
        
        // Update the network system
        networkSystem.update(1.0f / 60.0f);
        netComp->update(1.0f / 60.0f);
        
        // Display network stats
        float outBps, inBps, latency, packetLoss;
        networkSystem.getNetworkStats(outBps, inBps, latency, packetLoss);
        std::cout << "Network Stats - Out: " << outBps << " B/s, In: " << inBps 
                  << " B/s, Latency: " << latency << "ms, Loss: " << packetLoss << "%" << std::endl;
        
        // Sleep to simulate frame time
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Clean up
    networkSystem.unregisterNetworkedEntity(player);
    networkSystem.shutdown();
    
    LOG_INFO("Network Component Test Complete");
    return 0;
}
