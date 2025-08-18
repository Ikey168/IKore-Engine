#include "AIComponent.h"
#include "core/Entity.h"
#include "TransformComponent.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <random>

namespace IKore {

    AIComponent::AIComponent() 
        : Component()
        , m_state(AIState::INACTIVE)
        , m_currentWaypointIndex(0)
        , m_behaviorTimer(0.0f)
        , m_lastCooldownTime(0.0f)
        , m_updateFrequency(60.0f)
        , m_timeSinceLastUpdate(0.0f)
        , m_needsUpdate(true) {
    }

    AIComponent::~AIComponent() {
        // Cleanup any resources
    }

    void AIComponent::addBehavior(const std::string& name, const AIBehaviorConfig& config) {
        m_behaviors[name] = config;
        
        // If this is the first behavior and no active behavior is set, make it active
        if (m_activeBehaviorName.empty() && m_state == AIState::INACTIVE) {
            setActiveBehavior(name);
        }
    }

    void AIComponent::removeBehavior(const std::string& name) {
        auto it = m_behaviors.find(name);
        if (it != m_behaviors.end()) {
            // If removing the active behavior, switch to idle or another behavior
            if (m_activeBehaviorName == name) {
                if (m_behaviors.size() > 1) {
                    // Find another behavior to switch to
                    for (const auto& pair : m_behaviors) {
                        if (pair.first != name) {
                            setActiveBehavior(pair.first);
                            break;
                        }
                    }
                } else {
                    m_activeBehaviorName.clear();
                    m_state = AIState::INACTIVE;
                }
            }
            m_behaviors.erase(it);
        }
    }

    bool AIComponent::hasBehavior(const std::string& name) const {
        return m_behaviors.find(name) != m_behaviors.end();
    }

    void AIComponent::setActiveBehavior(const std::string& name) {
        if (m_behaviors.find(name) != m_behaviors.end()) {
            if (m_activeBehaviorName != name) {
                transitionToBehavior(name);
            }
        }
    }

    const AIBehaviorConfig* AIComponent::getCurrentBehaviorConfig() const {
        auto it = m_behaviors.find(m_activeBehaviorName);
        return (it != m_behaviors.end()) ? &it->second : nullptr;
    }

    void AIComponent::addEventListener(const std::string& eventType, const EventCallback& callback) {
        m_eventCallbacks[eventType] = callback;
    }

    void AIComponent::removeEventListener(const std::string& eventType) {
        m_eventCallbacks.erase(eventType);
    }

    void AIComponent::processEvent(const AIEvent& event) {
        auto it = m_eventCallbacks.find(event.type);
        if (it != m_eventCallbacks.end()) {
            std::string newBehavior = it->second(event);
            if (!newBehavior.empty() && hasBehavior(newBehavior)) {
                // Check if the new behavior has higher priority
                const auto* currentConfig = getCurrentBehaviorConfig();
                const auto* newConfig = &m_behaviors[newBehavior];
                
                if (!currentConfig || newConfig->priority > currentConfig->priority) {
                    setActiveBehavior(newBehavior);
                }
            }
        }
    }

    void AIComponent::update(float deltaTime) {
        if (m_state != AIState::ACTIVE) {
            return;
        }

        // Performance optimization: only update at specified frequency
        m_timeSinceLastUpdate += deltaTime;
        float updateInterval = 1.0f / m_updateFrequency;
        
        if (!m_needsUpdate && m_timeSinceLastUpdate < updateInterval) {
            return;
        }
        
        m_needsUpdate = false;
        m_timeSinceLastUpdate = 0.0f;

        // Update behavior timer
        m_behaviorTimer += deltaTime;

        // Get current behavior config
        const auto* config = getCurrentBehaviorConfig();
        if (!config) {
            return;
        }

        // Check if behavior has exceeded its duration
        if (config->duration > 0.0f && m_behaviorTimer >= config->duration) {
            // Behavior has expired, switch to idle or next priority behavior
            auto idleBehavior = m_behaviors.find("idle");
            if (idleBehavior != m_behaviors.end()) {
                setActiveBehavior("idle");
            } else {
                m_state = AIState::PAUSED;
            }
            return;
        }

        // Update entity position from transform component
        if (auto entity = getEntity().lock()) {
            if (auto transform = entity->getComponent<TransformComponent>()) {
                m_currentPosition = transform->getPosition();
            }
        }

        // Execute current behavior
        switch (config->type) {
            case AIBehaviorType::IDLE:
                updateIdleBehavior(deltaTime);
                break;
            case AIBehaviorType::PATROL:
                updatePatrolBehavior(deltaTime);
                break;
            case AIBehaviorType::CHASE:
                updateChaseBehavior(deltaTime);
                break;
            case AIBehaviorType::FLEE:
                updateFleeBehavior(deltaTime);
                break;
            case AIBehaviorType::GUARD:
                updateGuardBehavior(deltaTime);
                break;
            case AIBehaviorType::WANDER:
                updateWanderBehavior(deltaTime);
                break;
            case AIBehaviorType::CUSTOM:
                updateCustomBehavior(deltaTime);
                break;
        }
    }

    void AIComponent::setTarget(std::shared_ptr<Entity> target) {
        m_currentTarget = target;
        m_needsUpdate = true;
    }

    float AIComponent::getDistanceToTarget() const {
        auto target = m_currentTarget.lock();
        if (!target) {
            return std::numeric_limits<float>::max();
        }

        if (auto transform = target->getComponent<TransformComponent>()) {
            return glm::length(transform->getPosition() - m_currentPosition);
        }

        return std::numeric_limits<float>::max();
    }

    bool AIComponent::isTargetInRange(float range) const {
        return getDistanceToTarget() <= range;
    }

    const std::vector<std::string> AIComponent::getBehaviorNames() const {
        std::vector<std::string> names;
        names.reserve(m_behaviors.size());
        
        for (const auto& pair : m_behaviors) {
            names.push_back(pair.first);
        }
        
        return names;
    }

    void AIComponent::printDebugInfo() const {
        std::cout << "\n=== AI Component Debug Info ===" << std::endl;
        std::cout << "State: ";
        switch (m_state) {
            case AIState::INACTIVE: std::cout << "INACTIVE"; break;
            case AIState::ACTIVE: std::cout << "ACTIVE"; break;
            case AIState::TRANSITIONING: std::cout << "TRANSITIONING"; break;
            case AIState::PAUSED: std::cout << "PAUSED"; break;
        }
        std::cout << std::endl;
        
        std::cout << "Active Behavior: " << m_activeBehaviorName << std::endl;
        std::cout << "Position: (" << m_currentPosition.x << ", " << m_currentPosition.y << ", " << m_currentPosition.z << ")" << std::endl;
        std::cout << "Behavior Timer: " << m_behaviorTimer << "s" << std::endl;
        std::cout << "Update Frequency: " << m_updateFrequency << " Hz" << std::endl;
        std::cout << "Available Behaviors: ";
        
        for (const auto& pair : m_behaviors) {
            std::cout << pair.first << " ";
        }
        std::cout << std::endl;
        
        if (auto target = m_currentTarget.lock()) {
            std::cout << "Target Distance: " << getDistanceToTarget() << std::endl;
        } else {
            std::cout << "No Target" << std::endl;
        }
        
        std::cout << "===============================" << std::endl;
    }

    // Private behavior implementation methods

    void AIComponent::updateIdleBehavior(float deltaTime) {
        // Idle behavior does nothing - AI stays in place
        // This can be extended to include idle animations, looking around, etc.
    }

    void AIComponent::updatePatrolBehavior(float deltaTime) {
        const auto* config = getCurrentBehaviorConfig();
        if (!config || config->waypoints.empty()) {
            return;
        }

        // Get current waypoint
        if (m_currentWaypointIndex >= static_cast<int>(config->waypoints.size())) {
            if (config->loopBehavior) {
                m_currentWaypointIndex = 0;
            } else {
                // Patrol completed, switch to idle
                if (hasBehavior("idle")) {
                    setActiveBehavior("idle");
                }
                return;
            }
        }

        const glm::vec3& targetWaypoint = config->waypoints[m_currentWaypointIndex];
        
        // Move towards current waypoint
        if (isAtWaypoint(targetWaypoint)) {
            // Reached waypoint, move to next
            m_currentWaypointIndex++;
        } else {
            moveTowardsTarget(targetWaypoint, config->speed, deltaTime);
        }
    }

    void AIComponent::updateChaseBehavior(float deltaTime) {
        const auto* config = getCurrentBehaviorConfig();
        if (!config) {
            return;
        }

        auto target = m_currentTarget.lock();
        if (!target) {
            // Try to find a target if none is set
            target = findNearestEntity(config->detectionRange, "player");
            if (target) {
                setTarget(target);
            } else {
                // No target found, switch to patrol or idle
                if (hasBehavior("patrol")) {
                    setActiveBehavior("patrol");
                } else if (hasBehavior("idle")) {
                    setActiveBehavior("idle");
                }
                return;
            }
        }

        // Get target position
        if (auto targetTransform = target->getComponent<TransformComponent>()) {
            glm::vec3 targetPos = targetTransform->getPosition();
            float distance = glm::length(targetPos - m_currentPosition);

            // Check if target is out of detection range
            if (distance > config->detectionRange) {
                // Lost target, return to patrol or idle
                setTarget(nullptr);
                if (hasBehavior("patrol")) {
                    setActiveBehavior("patrol");
                } else if (hasBehavior("idle")) {
                    setActiveBehavior("idle");
                }
                return;
            }

            // Move towards target
            if (distance > config->actionRange) {
                moveTowardsTarget(targetPos, config->speed, deltaTime);
            }
            // If within action range, AI could perform an action (attack, interact, etc.)
        }
    }

    void AIComponent::updateFleeBehavior(float deltaTime) {
        const auto* config = getCurrentBehaviorConfig();
        if (!config) {
            return;
        }

        auto threat = m_currentTarget.lock();
        if (!threat) {
            // No threat, switch to idle or patrol
            if (hasBehavior("patrol")) {
                setActiveBehavior("patrol");
            } else if (hasBehavior("idle")) {
                setActiveBehavior("idle");
            }
            return;
        }

        // Get threat position and calculate flee direction
        if (auto threatTransform = threat->getComponent<TransformComponent>()) {
            glm::vec3 threatPos = threatTransform->getPosition();
            glm::vec3 fleeDirection = glm::normalize(m_currentPosition - threatPos);
            
            // Move away from threat
            glm::vec3 fleeTarget = m_currentPosition + fleeDirection * 10.0f; // Flee 10 units away
            moveTowardsTarget(fleeTarget, config->speed, deltaTime);
            
            // Check if far enough away
            float distance = glm::length(threatPos - m_currentPosition);
            if (distance > config->detectionRange) {
                // Safe distance reached
                setTarget(nullptr);
                if (hasBehavior("idle")) {
                    setActiveBehavior("idle");
                }
            }
        }
    }

    void AIComponent::updateGuardBehavior(float deltaTime) {
        const auto* config = getCurrentBehaviorConfig();
        if (!config || config->waypoints.empty()) {
            return;
        }

        // Guard behavior: stay near a specific position and watch for threats
        glm::vec3 guardPosition = config->waypoints[0]; // First waypoint is guard position
        float guardRadius = config->detectionRange;

        // Look for threats in the area
        auto threat = findNearestEntity(guardRadius, "enemy");
        if (threat) {
            // Found a threat, switch to chase behavior if available
            setTarget(threat);
            if (hasBehavior("chase")) {
                setActiveBehavior("chase");
                return;
            }
        }

        // Return to guard position if too far away
        float distanceFromGuardPos = glm::length(m_currentPosition - guardPosition);
        if (distanceFromGuardPos > guardRadius * 0.5f) {
            moveTowardsTarget(guardPosition, config->speed, deltaTime);
        }
    }

    void AIComponent::updateWanderBehavior(float deltaTime) {
        const auto* config = getCurrentBehaviorConfig();
        if (!config) {
            return;
        }

        // Simple random wandering within bounds
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        // Generate random movement direction every few seconds
        static float wanderTimer = 0.0f;
        static glm::vec3 wanderDirection(0.0f);
        
        wanderTimer += deltaTime;
        if (wanderTimer >= 2.0f) { // Change direction every 2 seconds
            wanderDirection = glm::normalize(glm::vec3(dis(gen), 0.0f, dis(gen)));
            wanderTimer = 0.0f;
        }

        // Move in wander direction
        glm::vec3 wanderTarget = m_currentPosition + wanderDirection * 5.0f;
        moveTowardsTarget(wanderTarget, config->speed * 0.5f, deltaTime); // Slower wandering
    }

    void AIComponent::updateCustomBehavior(float deltaTime) {
        // Custom behavior implementation can be extended by derived classes
        // or through behavior parameters
    }

    // Helper methods

    void AIComponent::transitionToBehavior(const std::string& behaviorName) {
        if (m_activeBehaviorName == behaviorName) {
            return;
        }

        m_state = AIState::TRANSITIONING;
        m_activeBehaviorName = behaviorName;
        m_behaviorTimer = 0.0f;
        m_currentWaypointIndex = 0;
        m_state = AIState::ACTIVE;
        m_needsUpdate = true;
    }

    glm::vec3 AIComponent::calculateMovementDirection(const glm::vec3& targetPosition) const {
        glm::vec3 direction = targetPosition - m_currentPosition;
        float length = glm::length(direction);
        
        if (length > 0.001f) {
            return direction / length;
        }
        
        return glm::vec3(0.0f);
    }

    bool AIComponent::isAtWaypoint(const glm::vec3& waypoint, float threshold) const {
        return glm::length(waypoint - m_currentPosition) <= threshold;
    }

    void AIComponent::moveTowardsTarget(const glm::vec3& target, float speed, float deltaTime) {
        glm::vec3 direction = calculateMovementDirection(target);
        glm::vec3 movement = direction * speed * deltaTime;
        
        // Update position through transform component if available
        if (auto entity = getEntity().lock()) {
            if (auto transform = entity->getComponent<TransformComponent>()) {
                glm::vec3 newPosition = transform->getPosition() + movement;
                transform->setPosition(newPosition);
                m_currentPosition = newPosition;
            }
        }
    }

    std::shared_ptr<Entity> AIComponent::findNearestEntity(float range, const std::string& tag) const {
        // This would typically query the entity manager for entities with specific tags
        // For now, return nullptr - this would need to be implemented with a proper entity query system
        return nullptr;
    }

    void AIComponent::onAttach() {
        Component::onAttach();
        m_state = AIState::INACTIVE;
        
        // If we have behaviors, activate the first one
        if (!m_behaviors.empty() && m_activeBehaviorName.empty()) {
            setActiveBehavior(m_behaviors.begin()->first);
        }
    }

    void AIComponent::onDetach() {
        m_state = AIState::INACTIVE;
        Component::onDetach();
    }

} // namespace IKore
