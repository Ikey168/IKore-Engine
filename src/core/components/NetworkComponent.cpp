#include "NetworkComponent.h"
#include "../Entity.h"
#include "../Logger.h"
#include <random>
#include <ctime>
#include <algorithm>
#include <thread>
#include <sstream>

namespace IKore {

NetworkComponent::NetworkComponent() {
    // Initialize with default values
}

NetworkComponent::~NetworkComponent() {
    // Clean up any network resources
    if (m_initialized) {
        // Close any open connections or clean up network resources
    }
}

bool NetworkComponent::initialize(NetworkRole role, const std::string& uniqueNetworkId) {
    m_role = role;
    m_networkId = uniqueNetworkId;
    
    // Initialize networking for this component based on role
    switch (m_role) {
        case NetworkRole::SERVER:
            {
                std::stringstream ss;
                ss << "Initializing network component as SERVER with ID: " << m_networkId;
                LOG_INFO(ss.str());
            }
            break;
            
        case NetworkRole::CLIENT:
            {
                std::stringstream ss;
                ss << "Initializing network component as CLIENT with ID: " << m_networkId;
                LOG_INFO(ss.str());
            }
            break;
            
        case NetworkRole::PEER:
            {
                std::stringstream ss;
                ss << "Initializing network component as PEER with ID: " << m_networkId;
                LOG_INFO(ss.str());
            }
            break;
            
        case NetworkRole::NONE:
        default:
            {
                std::stringstream ss;
                ss << "Initializing network component as LOCAL ONLY with ID: " << m_networkId;
                LOG_INFO(ss.str());
            }
            break;
    }
    
    m_initialized = true;
    return true;
}

void NetworkComponent::onAttach() {
    // Called when the component is attached to an entity
    LOG_INFO("Network component attached to entity");
}

void NetworkComponent::onDetach() {
    // Called when the component is detached from an entity
    LOG_INFO("Network component detached from entity");
    
    // Clean up any network resources
    if (m_initialized) {
        // Close connections, etc.
        m_initialized = false;
    }
}

bool NetworkComponent::registerProperty(const std::string& propertyName, 
                                        SyncMode syncMode, 
                                        float priority, 
                                        float updateFrequency, 
                                        bool reliable) {
    // Lock for thread safety when modifying property maps
    std::lock_guard<std::mutex> lock(m_propertyMutex);
    
    // Check if property already exists
    if (m_registeredProperties.find(propertyName) != m_registeredProperties.end()) {
        std::stringstream ss;
        ss << "Property '" << propertyName << "' already registered for network synchronization";
        LOG_WARNING(ss.str());
        return false;
    }
    
    // Create and register the property
    NetworkProperty prop;
    prop.name = propertyName;
    prop.syncMode = syncMode;
    prop.priority = priority;
    prop.updateFrequency = updateFrequency;
    prop.reliable = reliable;
    
    m_registeredProperties[propertyName] = prop;
    
    // Initialize last update time
    m_lastPropertyUpdateTime[propertyName] = std::chrono::steady_clock::now();
    
    std::stringstream ss;
    ss << "Property '" << propertyName << "' registered for network synchronization with mode: " 
       << static_cast<int>(syncMode) << ", priority: " << priority << ", frequency: " << updateFrequency << " Hz";
    LOG_INFO(ss.str());
             
    return true;
}

bool NetworkComponent::setPropertyUpdateCallback(const std::string& propertyName, 
                                               std::function<void(const std::vector<uint8_t>&)> callback) {
    // Lock for thread safety
    std::lock_guard<std::mutex> lock(m_propertyMutex);
    
    // Check if property exists
    if (m_registeredProperties.find(propertyName) == m_registeredProperties.end()) {
        std::stringstream ss;
        ss << "Cannot set callback for unregistered property: '" << propertyName << "'";
        LOG_WARNING(ss.str());
        return false;
    }
    
    // Set the callback
    m_propertyCallbacks[propertyName] = callback;
    
    std::stringstream ss;
    ss << "Property update callback set for '" << propertyName << "'";
    LOG_INFO(ss.str());
    return true;
}

bool NetworkComponent::sendPropertyUpdate(const std::string& propertyName, const std::vector<uint8_t>& data) {
    // Lock for thread safety
    std::lock_guard<std::mutex> lock(m_propertyMutex);
    
    // Check if property exists
    auto propIt = m_registeredProperties.find(propertyName);
    if (propIt == m_registeredProperties.end()) {
        std::stringstream ss;
        ss << "Cannot send update for unregistered property: '" << propertyName << "'";
        LOG_WARNING(ss.str());
        return false;
    }
    
    // Check if we have the authority to send this property based on our role
    if (m_role == NetworkRole::CLIENT && propIt->second.syncMode == SyncMode::AUTHORITATIVE) {
        std::stringstream ss;
        ss << "Client attempted to send authoritative property: '" << propertyName << "'";
        LOG_WARNING(ss.str());
        return false;
    }
    
    // Check if it's time to send this property based on update frequency
    auto now = std::chrono::steady_clock::now();
    auto lastUpdate = m_lastPropertyUpdateTime[propertyName];
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
    float minUpdateInterval = 1000.0f / propIt->second.updateFrequency; // convert Hz to ms
    
    if (elapsed < minUpdateInterval) {
        // Not time to update yet
        return false;
    }
    
    // Update the last update time
    m_lastPropertyUpdateTime[propertyName] = now;
    
    // Simulate packet loss for testing
    if (m_simulatedPacketLoss > 0.0f) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0, 100);
        if (dis(gen) < m_simulatedPacketLoss) {
            std::stringstream ss;
            ss << "Simulated packet loss for property: '" << propertyName << "'";
            LOG_INFO(ss.str());
            return true; // Pretend we sent it, but actually "lose" the packet
        }
    }
    
    // Simulate network latency for testing
    if (m_simulatedLatency > 0.0f) {
        std::stringstream ss;
        ss << "Simulating network latency of " << m_simulatedLatency << " ms for property: '" << propertyName << "'";
        LOG_INFO(ss.str());
        // In a real implementation, we would add this to a delayed send queue
        // For now, just sleep to simulate latency
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(m_simulatedLatency)));
    }
    
    std::stringstream ss;
    ss << "Sending property update for '" << propertyName << "' with " << data.size() << " bytes";
    LOG_INFO(ss.str());
    
    // In a real implementation, we would serialize and send the data here
    // For now, just log that we're sending the property
    return true;
}

void NetworkComponent::update(float deltaTime) {
    if (!m_initialized) return;
    
    // Process any incoming network messages
    // In a real implementation, this would check for and process new messages
    
    // Update property states based on role
    std::lock_guard<std::mutex> lock(m_propertyMutex);
    
    for (const auto& prop : m_registeredProperties) {
        // Check if it's time to update this property
        auto now = std::chrono::steady_clock::now();
        auto lastUpdate = m_lastPropertyUpdateTime[prop.first];
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
        float minUpdateInterval = 1000.0f / prop.second.updateFrequency; // convert Hz to ms
        
        // Different behavior based on sync mode
        switch (prop.second.syncMode) {
            case SyncMode::PREDICTIVE:
                // If we're the client, we should predict and then validate when server updates arrive
                if (m_role == NetworkRole::CLIENT) {
                    // Prediction logic would go here
                }
                break;
                
            case SyncMode::INTERPOLATED:
                // Interpolation logic would go here
                break;
                
            case SyncMode::AUTHORITATIVE:
            default:
                // For authoritative properties, only the server sends updates
                break;
        }
    }
}

} // namespace IKore
