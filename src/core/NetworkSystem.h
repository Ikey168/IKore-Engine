#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>

namespace IKore {

class Entity;
class NetworkComponent;

/**
 * @brief Message types for network communication
 */
enum class NetworkMessageType {
    CONNECT,         // Initial connection request
    DISCONNECT,      // Graceful disconnect
    ENTITY_CREATE,   // Create a new networked entity
    ENTITY_DESTROY,  // Destroy a networked entity
    PROPERTY_UPDATE, // Update a property on an entity
    SYSTEM_MESSAGE,  // System-level message
    CUSTOM_MESSAGE   // Application-defined message
};

/**
 * @brief Structure for network messages
 */
struct NetworkMessage {
    NetworkMessageType type;
    std::string entityId;
    std::string propertyName;
    std::vector<uint8_t> data;
    bool reliable;
    uint32_t sequence;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Network connection state
 */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
};

/**
 * @brief NetworkSystem - Manages network synchronization for entities
 * 
 * This system handles the network communication for all NetworkComponents:
 * - Manages client-server connections
 * - Routes messages to the appropriate entities
 * - Handles network discovery and initialization
 * - Provides bandwidth management and monitoring
 */
class NetworkSystem {
public:
    /**
     * @brief Get the singleton instance of the NetworkSystem
     * 
     * @return NetworkSystem& The singleton instance
     */
    static NetworkSystem& getInstance();
    
    /**
     * @brief Initialize the network system
     * 
     * @param isServer Whether this instance should act as a server
     * @param port The network port to use
     * @return bool True if initialization was successful
     */
    bool initialize(bool isServer, uint16_t port = 7777);
    
    /**
     * @brief Shutdown the network system
     */
    void shutdown();
    
    /**
     * @brief Update the network system
     * 
     * @param deltaTime Time since last update
     */
    void update(float deltaTime);
    
    /**
     * @brief Connect to a server
     * 
     * @param address The server address
     * @param port The server port
     * @return bool True if connection attempt was initiated successfully
     */
    bool connectToServer(const std::string& address, uint16_t port = 7777);
    
    /**
     * @brief Disconnect from the server
     * 
     * @param reason Optional reason for disconnection
     */
    void disconnect(const std::string& reason = "");
    
    /**
     * @brief Register an entity for network synchronization
     * 
     * @param entity The entity to register
     * @param networkId A unique identifier for this entity across the network
     * @return bool True if registration was successful
     */
    bool registerNetworkedEntity(std::shared_ptr<Entity> entity, const std::string& networkId = "");
    
    /**
     * @brief Unregister an entity from network synchronization
     * 
     * @param entity The entity to unregister
     */
    void unregisterNetworkedEntity(std::shared_ptr<Entity> entity);
    
    /**
     * @brief Send a network message
     * 
     * @param message The message to send
     * @return bool True if the message was queued for sending
     */
    bool sendMessage(const NetworkMessage& message);
    
    /**
     * @brief Set a callback for handling incoming messages
     * 
     * @param messageType The type of message to handle
     * @param callback The function to call when a message is received
     */
    void setMessageHandler(NetworkMessageType messageType, 
                          std::function<void(const NetworkMessage&)> callback);
    
    /**
     * @brief Get the current connection state
     * 
     * @return ConnectionState The current state
     */
    ConnectionState getConnectionState() const;
    
    /**
     * @brief Get network statistics
     * 
     * @param outBytesPerSecond Bytes sent per second
     * @param inBytesPerSecond Bytes received per second
     * @param latencyMs Round-trip latency in milliseconds
     * @param packetLoss Packet loss percentage
     */
    void getNetworkStats(float& outBytesPerSecond, 
                         float& inBytesPerSecond, 
                         float& latencyMs, 
                         float& packetLoss);
    
    /**
     * @brief Set bandwidth limits
     * 
     * @param maxOutBytesPerSecond Maximum outgoing bytes per second (0 = unlimited)
     * @param maxInBytesPerSecond Maximum incoming bytes per second (0 = unlimited)
     */
    void setBandwidthLimits(uint32_t maxOutBytesPerSecond, uint32_t maxInBytesPerSecond);
    
private:
    NetworkSystem();
    ~NetworkSystem();
    
    // Prevent copying
    NetworkSystem(const NetworkSystem&) = delete;
    NetworkSystem& operator=(const NetworkSystem&) = delete;
    
    // Worker thread functions
    void sendWorkerThread();
    void receiveWorkerThread();
    
    // Process messages in the incoming queue
    void processIncomingMessages();
    
    // Helper methods
    std::string generateUniqueNetworkId();
    
    // Member variables
    bool m_isServer{false};
    uint16_t m_port{7777};
    std::atomic<ConnectionState> m_connectionState{ConnectionState::DISCONNECTED};
    
    // Network statistics
    std::atomic<uint32_t> m_bytesSent{0};
    std::atomic<uint32_t> m_bytesReceived{0};
    std::atomic<float> m_currentLatency{0.0f};
    std::atomic<float> m_packetLoss{0.0f};
    uint32_t m_maxOutBytesPerSecond{0}; // 0 = unlimited
    uint32_t m_maxInBytesPerSecond{0};  // 0 = unlimited
    
    // Entity management
    std::unordered_map<std::string, std::weak_ptr<Entity>> m_networkedEntities;
    std::mutex m_entityMutex;
    
    // Message handlers
    std::unordered_map<NetworkMessageType, std::function<void(const NetworkMessage&)>> m_messageHandlers;
    std::mutex m_handlerMutex;
    
    // Message queues
    std::queue<NetworkMessage> m_outgoingMessages;
    std::mutex m_outgoingMutex;
    std::condition_variable m_outgoingCondition;
    
    std::queue<NetworkMessage> m_incomingMessages;
    std::mutex m_incomingMutex;
    
    // Worker threads
    std::thread m_sendThread;
    std::thread m_receiveThread;
    std::atomic<bool> m_running{false};
};

} // namespace IKore
