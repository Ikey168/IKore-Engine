#pragma once

#include "../Component.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>
#include <chrono>

namespace IKore {

    /**
     * @brief Enumeration for network roles
     */
    enum class NetworkRole {
        NONE,      // Not participating in network (local only)
        SERVER,    // Server authority
        CLIENT,    // Client with server validation
        PEER       // Peer-to-peer (shared authority)
    };

    /**
     * @brief Enumeration for network property synchronization modes
     */
    enum class SyncMode {
        AUTHORITATIVE, // Server has final say
        PREDICTIVE,    // Client predicts, server validates
        INTERPOLATED   // Smooth interpolation between updates
    };

    /**
     * @brief Structure for network property metadata
     */
    struct NetworkProperty {
        std::string name;
        SyncMode syncMode;
        float priority;        // Higher priority properties are sent more frequently
        float updateFrequency; // How often this property should be synced (in Hz)
        bool reliable;         // Should this property use reliable delivery
    };

    /**
     * @brief NetworkComponent - Provides network synchronization for entity properties
     * 
     * This component enables an entity to be synchronized across a network:
     * - Supports client-server architecture
     * - Property replication with different synchronization modes
     * - Bandwidth optimization with update frequency controls
     * - Lag compensation and prediction
     * 
     * Usage Examples:
     * 
     * @code
     * // Create a networked player character
     * auto player = std::make_shared<Entity>();
     * auto network = player->addComponent<NetworkComponent>();
     * network->setRole(NetworkRole::SERVER);
     * network->registerProperty("position", SyncMode::PREDICTIVE, 10.0f, 20.0f, true);
     * @endcode
     */
    class NetworkComponent : public Component {
    public:
        NetworkComponent();
        virtual ~NetworkComponent();
        
        /**
         * @brief Initialize the network component
         * 
         * @param role The network role this entity should have
         * @param uniqueNetworkId A unique identifier for this entity across the network
         * @return bool True if initialization was successful
         */
        bool initialize(NetworkRole role, const std::string& uniqueNetworkId);
        
        /**
         * @brief Set the network role of this entity
         * 
         * @param role The network role (SERVER, CLIENT, PEER)
         */
        void setRole(NetworkRole role) { m_role = role; }
        
        /**
         * @brief Get the network role of this entity
         * 
         * @return NetworkRole The current network role
         */
        NetworkRole getRole() const { return m_role; }
        
        /**
         * @brief Register a property for network synchronization
         * 
         * @param propertyName Name of the property
         * @param syncMode How the property should be synchronized
         * @param priority Higher priority properties are sent more frequently
         * @param updateFrequency How often this property should be synced (in Hz)
         * @param reliable Whether this property should use reliable delivery
         * @return bool True if property was registered successfully
         */
        bool registerProperty(const std::string& propertyName, 
                              SyncMode syncMode,
                              float priority = 1.0f,
                              float updateFrequency = 10.0f, 
                              bool reliable = true);
        
        /**
         * @brief Set a callback for when a property is received from the network
         * 
         * @param propertyName Name of the property
         * @param callback Function to call when property is updated
         * @return bool True if callback was set successfully
         */
        bool setPropertyUpdateCallback(const std::string& propertyName, 
                                      std::function<void(const std::vector<uint8_t>&)> callback);
        
        /**
         * @brief Send a property update to the network
         * 
         * @param propertyName Name of the property
         * @param data Serialized property data
         * @return bool True if update was sent successfully
         */
        bool sendPropertyUpdate(const std::string& propertyName, const std::vector<uint8_t>& data);
        
        /**
         * @brief Process incoming network messages
         * 
         * @param deltaTime Time since last update
         */
        void update(float deltaTime);
        
        /**
         * @brief Get the unique network ID for this entity
         * 
         * @return const std::string& The network ID
         */
        const std::string& getNetworkId() const { return m_networkId; }
        
        /**
         * @brief Set the network latency simulation for testing
         * 
         * @param latencyMs Simulated network latency in milliseconds
         */
        void setSimulatedLatency(float latencyMs) { m_simulatedLatency = latencyMs; }
        
        /**
         * @brief Set the packet loss simulation for testing
         * 
         * @param packetLossPercent Simulated packet loss percentage (0-100)
         */
        void setSimulatedPacketLoss(float packetLossPercent) { m_simulatedPacketLoss = packetLossPercent; }
        
        /**
         * @brief Enable or disable lag compensation
         * 
         * @param enabled Whether lag compensation should be enabled
         */
        void setLagCompensationEnabled(bool enabled) { m_lagCompensationEnabled = enabled; }
        
    protected:
        virtual void onAttach() override;
        virtual void onDetach() override;
        
    private:
        NetworkRole m_role{NetworkRole::NONE};
        std::string m_networkId;
        bool m_initialized{false};
        
        // Network simulation properties for testing
        float m_simulatedLatency{0.0f};       // in milliseconds
        float m_simulatedPacketLoss{0.0f};    // percentage
        bool m_lagCompensationEnabled{true};
        
        // Property management
        std::unordered_map<std::string, NetworkProperty> m_registeredProperties;
        std::unordered_map<std::string, std::function<void(const std::vector<uint8_t>&)>> m_propertyCallbacks;
        
        // Synchronization timing
        std::unordered_map<std::string, std::chrono::time_point<std::chrono::steady_clock>> m_lastPropertyUpdateTime;
        
        // Thread safety for property updates
        std::mutex m_propertyMutex;
    };
}
