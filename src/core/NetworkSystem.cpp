#include "NetworkSystem.h"
#include "Entity.h"
#include "components/NetworkComponent.h"
#include "Logger.h"
#include <chrono>
#include <random>
#include <algorithm>
#include <sstream>

namespace IKore {

NetworkSystem::NetworkSystem() {
    // Initialize with default values
}

NetworkSystem::~NetworkSystem() {
    // Ensure shutdown is called
    shutdown();
}

NetworkSystem& NetworkSystem::getInstance() {
    static NetworkSystem instance;
    return instance;
}

bool NetworkSystem::initialize(bool isServer, uint16_t port) {
    // Check if already initialized
    if (m_running) {
        LOG_WARNING("NetworkSystem already initialized");
        return false;
    }
    
    m_isServer = isServer;
    m_port = port;
    
    std::stringstream ss;
    ss << "Initializing NetworkSystem as " << (m_isServer ? "SERVER" : "CLIENT") 
       << " on port " << m_port;
    LOG_INFO(ss.str());
    
    // Reset statistics
    m_bytesSent = 0;
    m_bytesReceived = 0;
    m_currentLatency = 0.0f;
    m_packetLoss = 0.0f;
    
    // Set running flag and start worker threads
    m_running = true;
    m_connectionState = m_isServer ? ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
    
    // Start worker threads
    m_sendThread = std::thread(&NetworkSystem::sendWorkerThread, this);
    m_receiveThread = std::thread(&NetworkSystem::receiveWorkerThread, this);
    
    LOG_INFO("NetworkSystem initialized successfully");
    return true;
}

void NetworkSystem::shutdown() {
    if (!m_running) return;
    
    LOG_INFO("Shutting down NetworkSystem");
    
    // Signal threads to stop
    m_running = false;
    
    // Notify any waiting threads
    m_outgoingCondition.notify_all();
    
    // Wait for threads to finish
    if (m_sendThread.joinable()) {
        m_sendThread.join();
    }
    
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
    
    // Clear message queues
    {
        std::lock_guard<std::mutex> lock(m_outgoingMutex);
        while (!m_outgoingMessages.empty()) {
            m_outgoingMessages.pop();
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(m_incomingMutex);
        while (!m_incomingMessages.empty()) {
            m_incomingMessages.pop();
        }
    }
    
    // Clear entity references
    {
        std::lock_guard<std::mutex> lock(m_entityMutex);
        m_networkedEntities.clear();
    }
    
    m_connectionState = ConnectionState::DISCONNECTED;
    
    LOG_INFO("NetworkSystem shutdown complete");
}

void NetworkSystem::update(float deltaTime) {
    if (!m_running) return;
    
    // Process any incoming messages
    processIncomingMessages();
    
    // Update network statistics
    // (In a real implementation, we would calculate bytes per second, etc.)
    
    // Update all networked entities
    std::lock_guard<std::mutex> lock(m_entityMutex);
    
    // Clean up any expired entity references
    auto it = m_networkedEntities.begin();
    while (it != m_networkedEntities.end()) {
        if (it->second.expired()) {
            std::stringstream ss;
            ss << "Removing expired entity reference: " << it->first;
            LOG_INFO(ss.str());
            it = m_networkedEntities.erase(it);
        } else {
            ++it;
        }
    }
}

bool NetworkSystem::connectToServer(const std::string& address, uint16_t port) {
    if (m_isServer) {
        LOG_ERROR("Cannot connect to server: this instance is configured as a server");
        return false;
    }
    
    if (m_connectionState != ConnectionState::DISCONNECTED) {
        LOG_WARNING("Cannot connect: already connected or connecting");
        return false;
    }
    
    std::stringstream ss;
    ss << "Connecting to server at " << address << ":" << port;
    LOG_INFO(ss.str());
    
    // Set connection state to connecting
    m_connectionState = ConnectionState::CONNECTING;
    
    // In a real implementation, we would initiate the connection here
    // For now, just simulate a successful connection after a delay
    
    // Create a connection message
    NetworkMessage connectMsg;
    connectMsg.type = NetworkMessageType::CONNECT;
    connectMsg.reliable = true;
    connectMsg.timestamp = std::chrono::steady_clock::now();
    
    // Queue the message
    bool result = sendMessage(connectMsg);
    
    // For simulation purposes, set to connected immediately
    // In a real implementation, this would happen after receiving a connection confirmation
    m_connectionState = ConnectionState::CONNECTED;
    
    std::stringstream ss2;
    ss2 << "Connected to server at " << address << ":" << port;
    LOG_INFO(ss2.str());
    return result;
}

void NetworkSystem::disconnect(const std::string& reason) {
    if (m_connectionState != ConnectionState::CONNECTED) {
        LOG_WARNING("Cannot disconnect: not connected");
        return;
    }
    
    std::stringstream ss;
    ss << "Disconnecting from network. Reason: " << (reason.empty() ? "No reason provided" : reason);
    LOG_INFO(ss.str());
    
    // Set connection state to disconnecting
    m_connectionState = ConnectionState::DISCONNECTING;
    
    // Create a disconnect message
    NetworkMessage disconnectMsg;
    disconnectMsg.type = NetworkMessageType::DISCONNECT;
    disconnectMsg.reliable = true;
    disconnectMsg.timestamp = std::chrono::steady_clock::now();
    
    // Add reason to data if provided
    if (!reason.empty()) {
        std::copy(reason.begin(), reason.end(), std::back_inserter(disconnectMsg.data));
    }
    
    // Queue the message
    sendMessage(disconnectMsg);
    
    // For simulation purposes, set to disconnected immediately
    // In a real implementation, this would happen after confirmation
    m_connectionState = ConnectionState::DISCONNECTED;
    
    LOG_INFO("Disconnected from network");
}

bool NetworkSystem::registerNetworkedEntity(std::shared_ptr<Entity> entity, const std::string& networkId) {
    if (!entity) {
        LOG_ERROR("Cannot register null entity");
        return false;
    }
    
    // Generate a network ID if one wasn't provided
    std::string entityNetworkId = networkId;
    if (entityNetworkId.empty()) {
        entityNetworkId = generateUniqueNetworkId();
    }
    
    // Check if the entity already has a NetworkComponent
    auto networkComp = entity->getComponent<NetworkComponent>();
    if (!networkComp) {
        // Add a NetworkComponent if it doesn't have one
        networkComp = entity->addComponent<NetworkComponent>();
        if (!networkComp) {
            LOG_ERROR("Failed to add NetworkComponent to entity");
            return false;
        }
    }
    
    // Initialize the NetworkComponent
    NetworkRole role = m_isServer ? NetworkRole::SERVER : NetworkRole::CLIENT;
    if (!networkComp->initialize(role, entityNetworkId)) {
        LOG_ERROR("Failed to initialize NetworkComponent for entity");
        return false;
    }
    
    // Register the entity
    {
        std::lock_guard<std::mutex> lock(m_entityMutex);
        m_networkedEntities[entityNetworkId] = entity;
    }
    
    std::stringstream ss;
    ss << "Entity registered for network synchronization with ID: " << entityNetworkId;
    LOG_INFO(ss.str());
    
    // If we're the server, broadcast entity creation to clients
    if (m_isServer && m_connectionState == ConnectionState::CONNECTED) {
        NetworkMessage createMsg;
        createMsg.type = NetworkMessageType::ENTITY_CREATE;
        createMsg.entityId = entityNetworkId;
        createMsg.reliable = true;
        createMsg.timestamp = std::chrono::steady_clock::now();
        
        // Entity serialization would go here
        // createMsg.data = SerializeEntity(entity);
        
        sendMessage(createMsg);
    }
    
    return true;
}

void NetworkSystem::unregisterNetworkedEntity(std::shared_ptr<Entity> entity) {
    if (!entity) return;
    
    auto networkComp = entity->getComponent<NetworkComponent>();
    if (!networkComp) return;
    
    std::string entityNetworkId = networkComp->getNetworkId();
    
    // Remove from networked entities
    {
        std::lock_guard<std::mutex> lock(m_entityMutex);
        m_networkedEntities.erase(entityNetworkId);
    }
    
    std::stringstream ss;
    ss << "Entity unregistered from network synchronization: " << entityNetworkId;
    LOG_INFO(ss.str());
    
    // If we're the server, broadcast entity destruction to clients
    if (m_isServer && m_connectionState == ConnectionState::CONNECTED) {
        NetworkMessage destroyMsg;
        destroyMsg.type = NetworkMessageType::ENTITY_DESTROY;
        destroyMsg.entityId = entityNetworkId;
        destroyMsg.reliable = true;
        destroyMsg.timestamp = std::chrono::steady_clock::now();
        
        sendMessage(destroyMsg);
    }
}

bool NetworkSystem::sendMessage(const NetworkMessage& message) {
    if (!m_running) return false;
    
    // In a real implementation, we would check bandwidth limits here
    
    // Queue the message
    {
        std::lock_guard<std::mutex> lock(m_outgoingMutex);
        m_outgoingMessages.push(message);
    }
    
    // Notify the send thread
    m_outgoingCondition.notify_one();
    
    return true;
}

void NetworkSystem::setMessageHandler(NetworkMessageType messageType, 
                                     std::function<void(const NetworkMessage&)> callback) {
    if (!callback) return;
    
    std::lock_guard<std::mutex> lock(m_handlerMutex);
    m_messageHandlers[messageType] = callback;
}

ConnectionState NetworkSystem::getConnectionState() const {
    return m_connectionState;
}

void NetworkSystem::getNetworkStats(float& outBytesPerSecond, 
                                   float& inBytesPerSecond, 
                                   float& latencyMs, 
                                   float& packetLoss) {
    // In a real implementation, we would calculate these values over time
    // For now, just return the current values
    outBytesPerSecond = static_cast<float>(m_bytesSent);
    inBytesPerSecond = static_cast<float>(m_bytesReceived);
    latencyMs = m_currentLatency;
    packetLoss = m_packetLoss;
}

void NetworkSystem::setBandwidthLimits(uint32_t maxOutBytesPerSecond, uint32_t maxInBytesPerSecond) {
    m_maxOutBytesPerSecond = maxOutBytesPerSecond;
    m_maxInBytesPerSecond = maxInBytesPerSecond;
    
    std::stringstream ss;
    ss << "Bandwidth limits set - Out: " << m_maxOutBytesPerSecond 
       << " bytes/sec, In: " << m_maxInBytesPerSecond << " bytes/sec";
    LOG_INFO(ss.str());
}

void NetworkSystem::sendWorkerThread() {
    LOG_INFO("Send worker thread started");
    
    while (m_running) {
        NetworkMessage message;
        bool hasMessage = false;
        
        // Wait for a message to send
        {
            std::unique_lock<std::mutex> lock(m_outgoingMutex);
            
            // Wait for a message or shutdown signal
            m_outgoingCondition.wait(lock, [this] {
                return !m_running || !m_outgoingMessages.empty();
            });
            
            if (!m_running) break;
            
            if (!m_outgoingMessages.empty()) {
                message = m_outgoingMessages.front();
                m_outgoingMessages.pop();
                hasMessage = true;
            }
        }
        
        if (hasMessage) {
            // In a real implementation, we would send the message over the network here
            // For now, just log it and increment bytes sent
            
            // Calculate message size (header + data)
            size_t messageSize = sizeof(NetworkMessageType) + 
                                 message.entityId.size() + 
                                 message.propertyName.size() + 
                                 message.data.size() + 
                                 sizeof(message.reliable) + 
                                 sizeof(message.sequence);
            
            m_bytesSent += static_cast<uint32_t>(messageSize);
            
            std::stringstream ss;
            ss << "Sent message: Type=" << static_cast<int>(message.type)
               << ", Entity=" << message.entityId
               << ", Property=" << message.propertyName
               << ", Size=" << messageSize << " bytes";
            LOG_INFO(ss.str());
            
            // For simulation purposes, also add to incoming queue if we're in local mode
            if (m_isServer) {
                // Server would broadcast to clients
                // For simulation, just process it locally
                {
                    std::lock_guard<std::mutex> lock(m_incomingMutex);
                    m_incomingMessages.push(message);
                }
            }
        }
    }
    
    LOG_INFO("Send worker thread stopped");
}

void NetworkSystem::receiveWorkerThread() {
    LOG_INFO("Receive worker thread started");
    
    // Set up random number generator for simulating packet loss and latency
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> latencyDis(10.0, 100.0); // 10-100ms latency
    
    while (m_running) {
        // In a real implementation, we would receive messages from the network here
        // For now, just simulate receiving messages
        
        // Sleep to simulate network polling interval
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Simulate receiving a message (only in connected state)
        if (m_connectionState == ConnectionState::CONNECTED) {
            // Simulate latency
            m_currentLatency = static_cast<float>(latencyDis(gen));
            
            // In a real implementation, we would process actual received messages
            // and queue them in the incoming message queue
        }
    }
    
    LOG_INFO("Receive worker thread stopped");
}

void NetworkSystem::processIncomingMessages() {
    std::vector<NetworkMessage> messagesToProcess;
    
    // Get messages from the incoming queue
    {
        std::lock_guard<std::mutex> lock(m_incomingMutex);
        
        while (!m_incomingMessages.empty()) {
            messagesToProcess.push_back(m_incomingMessages.front());
            m_incomingMessages.pop();
        }
    }
    
    // Process each message
    for (const auto& message : messagesToProcess) {
        // Calculate message size
        size_t messageSize = sizeof(NetworkMessageType) + 
                             message.entityId.size() + 
                             message.propertyName.size() + 
                             message.data.size() + 
                             sizeof(message.reliable) + 
                             sizeof(message.sequence);
        
        m_bytesReceived += static_cast<uint32_t>(messageSize);
        
        std::stringstream ss;
        ss << "Processing message: Type=" << static_cast<int>(message.type)
           << ", Entity=" << message.entityId
           << ", Property=" << message.propertyName
           << ", Size=" << messageSize << " bytes";
        LOG_INFO(ss.str());
        
        // Handle system messages
        switch (message.type) {
            case NetworkMessageType::CONNECT:
                LOG_INFO("Received connection request");
                break;
                
            case NetworkMessageType::DISCONNECT:
                LOG_INFO("Received disconnect notification");
                break;
                
            case NetworkMessageType::ENTITY_CREATE:
                {
                    std::stringstream ss;
                    ss << "Received entity creation: " << message.entityId;
                    LOG_INFO(ss.str());
                }
                break;
                
            case NetworkMessageType::ENTITY_DESTROY:
                {
                    std::stringstream ss;
                    ss << "Received entity destruction: " << message.entityId;
                    LOG_INFO(ss.str());
                }
                break;
                
            case NetworkMessageType::PROPERTY_UPDATE:
                // Find the entity and update its property
                if (!message.entityId.empty() && !message.propertyName.empty()) {
                    std::shared_ptr<Entity> entity;
                    
                    {
                        std::lock_guard<std::mutex> lock(m_entityMutex);
                        auto it = m_networkedEntities.find(message.entityId);
                        if (it != m_networkedEntities.end() && !it->second.expired()) {
                            entity = it->second.lock();
                        }
                    }
                    
                    if (entity) {
                        auto networkComp = entity->getComponent<NetworkComponent>();
                        if (networkComp) {
                            std::stringstream ss;
                            ss << "Updating property '" << message.propertyName 
                               << "' on entity " << message.entityId;
                            LOG_INFO(ss.str());
                            
                            // In a real implementation, we would deserialize and apply the property
                        }
                    }
                }
                break;
                
            default:
                // Check for a custom message handler
                {
                    std::lock_guard<std::mutex> lock(m_handlerMutex);
                    auto it = m_messageHandlers.find(message.type);
                    if (it != m_messageHandlers.end()) {
                        // Call the handler
                        it->second(message);
                    }
                }
                break;
        }
    }
}

std::string NetworkSystem::generateUniqueNetworkId() {
    // Generate a unique ID for a networked entity
    static std::atomic<uint64_t> nextId{1};
    uint64_t id = nextId++;
    
    // Create an ID with a prefix based on server/client
    std::string prefix = m_isServer ? "S" : "C";
    return prefix + std::to_string(id);
}

} // namespace IKore
