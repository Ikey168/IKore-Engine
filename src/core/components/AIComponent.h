#pragma once

#include "core/Component.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <chrono>

namespace IKore {

    // Forward declarations
    class Entity;

    /**
     * @brief AI Behavior Types
     */
    enum class AIBehaviorType {
        IDLE,           // AI does nothing, stays in place
        PATROL,         // AI follows a set of waypoints
        CHASE,          // AI pursues a target entity
        FLEE,           // AI runs away from a target entity
        GUARD,          // AI protects a specific area or entity
        WANDER,         // AI moves randomly within bounds
        CUSTOM          // User-defined behavior
    };

    /**
     * @brief AI State for behavior management
     */
    enum class AIState {
        INACTIVE,       // AI is disabled
        ACTIVE,         // AI is running normally
        TRANSITIONING,  // AI is changing between behaviors
        PAUSED          // AI is temporarily stopped
    };

    /**
     * @brief AI Event data for event-driven interactions
     */
    struct AIEvent {
        std::string type;                           // Event type identifier
        std::weak_ptr<Entity> source;              // Entity that triggered the event
        glm::vec3 position{0.0f};                  // Event position in world space
        float radius{0.0f};                        // Event influence radius
        float intensity{1.0f};                     // Event strength/priority
        std::unordered_map<std::string, float> data; // Additional event data
        std::chrono::steady_clock::time_point timestamp; // When the event occurred
    };

    /**
     * @brief AI Behavior Configuration
     */
    struct AIBehaviorConfig {
        AIBehaviorType type{AIBehaviorType::IDLE};
        float priority{1.0f};                      // Behavior priority (higher = more important)
        float duration{-1.0f};                     // Max duration (-1 = infinite)
        float cooldown{0.0f};                      // Cooldown before behavior can restart
        std::vector<glm::vec3> waypoints;          // For patrol/guard behaviors
        std::weak_ptr<Entity> targetEntity;       // For chase/flee behaviors
        float speed{1.0f};                         // Movement speed multiplier
        float detectionRange{10.0f};              // Range for detecting targets/events
        float actionRange{2.0f};                  // Range for taking action
        bool loopBehavior{true};                   // Whether to loop the behavior
        std::unordered_map<std::string, float> parameters; // Custom behavior parameters
    };

    /**
     * @brief AI Component for basic entity behavior
     * 
     * This component provides AI capabilities to entities, including:
     * - Multiple behavior types (patrol, chase, flee, etc.)
     * - Event-driven decision making
     * - Smooth state transitions
     * - Efficient processing for multiple AI entities
     * 
     * Features:
     * - Behavior prioritization and switching
     * - Waypoint-based navigation
     * - Target tracking and pursuit
     * - Event detection and response
     * - Performance optimized updates
     * 
     * Usage Examples:
     * 
     * @code
     * // Add AI to an entity
     * auto aiComponent = entity->addComponent<AIComponent>();
     * 
     * // Set up patrol behavior
     * AIBehaviorConfig patrolConfig;
     * patrolConfig.type = AIBehaviorType::PATROL;
     * patrolConfig.waypoints = {
     *     glm::vec3(0, 0, 0),
     *     glm::vec3(10, 0, 0),
     *     glm::vec3(10, 0, 10),
     *     glm::vec3(0, 0, 10)
     * };
     * patrolConfig.speed = 2.0f;
     * aiComponent->addBehavior("patrol", patrolConfig);
     * aiComponent->setActiveBehavior("patrol");
     * 
     * // Set up chase behavior with higher priority
     * AIBehaviorConfig chaseConfig;
     * chaseConfig.type = AIBehaviorType::CHASE;
     * chaseConfig.priority = 5.0f;
     * chaseConfig.speed = 4.0f;
     * chaseConfig.detectionRange = 15.0f;
     * aiComponent->addBehavior("chase", chaseConfig);
     * 
     * // Register event listener
     * aiComponent->addEventListener("player_detected", 
     *     [](const AIEvent& event) {
     *         // Switch to chase behavior when player detected
     *         return "chase";
     *     });
     * @endcode
     */
    class AIComponent : public Component {
    public:
        AIComponent();
        virtual ~AIComponent();

        // Behavior Management
        void addBehavior(const std::string& name, const AIBehaviorConfig& config);
        void removeBehavior(const std::string& name);
        bool hasBehavior(const std::string& name) const;
        void setActiveBehavior(const std::string& name);
        const std::string& getActiveBehavior() const { return m_activeBehaviorName; }
        const AIBehaviorConfig* getCurrentBehaviorConfig() const;

        // State Management
        void setState(AIState state) { m_state = state; }
        AIState getState() const { return m_state; }
        bool isActive() const { return m_state == AIState::ACTIVE; }
        void pause() { m_state = AIState::PAUSED; }
        void resume() { m_state = AIState::ACTIVE; }

        // Event System
        using EventCallback = std::function<std::string(const AIEvent&)>; // Returns behavior name to switch to (empty = no change)
        void addEventListener(const std::string& eventType, const EventCallback& callback);
        void removeEventListener(const std::string& eventType);
        void processEvent(const AIEvent& event);

        // Behavior Execution
        void update(float deltaTime);
        void setPosition(const glm::vec3& position) { m_currentPosition = position; }
        const glm::vec3& getPosition() const { return m_currentPosition; }
        void setTarget(std::shared_ptr<Entity> target);
        std::shared_ptr<Entity> getTarget() const { return m_currentTarget.lock(); }

        // Debug and Information
        float getDistanceToTarget() const;
        bool isTargetInRange(float range) const;
        const std::vector<std::string> getBehaviorNames() const;
        void printDebugInfo() const;

        // Performance Settings
        void setUpdateFrequency(float frequency) { m_updateFrequency = frequency; }
        float getUpdateFrequency() const { return m_updateFrequency; }

    private:
        // Core state
        AIState m_state{AIState::INACTIVE};
        std::string m_activeBehaviorName;
        std::unordered_map<std::string, AIBehaviorConfig> m_behaviors;
        
        // Current behavior state
        glm::vec3 m_currentPosition{0.0f};
        std::weak_ptr<Entity> m_currentTarget;
        int m_currentWaypointIndex{0};
        float m_behaviorTimer{0.0f};
        float m_lastCooldownTime{0.0f};
        
        // Event system
        std::unordered_map<std::string, EventCallback> m_eventCallbacks;
        
        // Performance optimization
        float m_updateFrequency{60.0f};        // Updates per second
        float m_timeSinceLastUpdate{0.0f};
        bool m_needsUpdate{true};

        // Behavior implementation methods
        void updateIdleBehavior(float deltaTime);
        void updatePatrolBehavior(float deltaTime);
        void updateChaseBehavior(float deltaTime);
        void updateFleeBehavior(float deltaTime);
        void updateGuardBehavior(float deltaTime);
        void updateWanderBehavior(float deltaTime);
        void updateCustomBehavior(float deltaTime);

        // Helper methods
        void transitionToBehavior(const std::string& behaviorName);
        glm::vec3 calculateMovementDirection(const glm::vec3& targetPosition) const;
        bool isAtWaypoint(const glm::vec3& waypoint, float threshold = 1.0f) const;
        void moveTowardsTarget(const glm::vec3& target, float speed, float deltaTime);
        std::shared_ptr<Entity> findNearestEntity(float range, const std::string& tag = "") const;

    protected:
        void onAttach() override;
        void onDetach() override;
    };

} // namespace IKore
