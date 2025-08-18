#include "AISystem.h"
#include "components/AIComponent.h"
#include "components/TransformComponent.h"
#include <iostream>
#include <algorithm>

namespace IKore {

    AISystem::AISystem() 
        : m_maxUpdatesPerFrame(50)
        , m_currentUpdateIndex(0)
        , m_globalUpdateFrequency(30.0f)
        , m_debugMode(false)
        , m_lastFrameUpdates(0) {
    }

    AISystem::~AISystem() {
        m_aiEntities.clear();
        m_globalTargets.clear();
    }

    void AISystem::registerAIEntity(std::shared_ptr<Entity> entity) {
        if (!entity) {
            return;
        }

        // Check if entity has AI component
        auto aiComponent = entity->getComponent<AIComponent>();
        if (!aiComponent) {
            if (m_debugMode) {
                std::cout << "Warning: Attempting to register entity without AIComponent" << std::endl;
            }
            return;
        }

        // Check if already registered
        auto it = std::find_if(m_aiEntities.begin(), m_aiEntities.end(),
            [entity](const std::weak_ptr<Entity>& weak_entity) {
                return weak_entity.lock() == entity;
            });

        if (it == m_aiEntities.end()) {
            m_aiEntities.push_back(entity);
            
            // Set the global update frequency for this AI
            aiComponent->setUpdateFrequency(m_globalUpdateFrequency);
            
            if (m_debugMode) {
                std::cout << "Registered AI entity. Total AI entities: " << m_aiEntities.size() << std::endl;
            }
        }
    }

    void AISystem::unregisterAIEntity(std::shared_ptr<Entity> entity) {
        if (!entity) {
            return;
        }

        auto it = std::remove_if(m_aiEntities.begin(), m_aiEntities.end(),
            [entity](const std::weak_ptr<Entity>& weak_entity) {
                auto locked = weak_entity.lock();
                return !locked || locked == entity;
            });

        if (it != m_aiEntities.end()) {
            m_aiEntities.erase(it, m_aiEntities.end());
            
            if (m_debugMode) {
                std::cout << "Unregistered AI entity. Total AI entities: " << m_aiEntities.size() << std::endl;
            }
        }
    }

    void AISystem::updateAll(float deltaTime) {
        if (m_aiEntities.empty()) {
            return;
        }

        // Clean up invalid entities periodically
        static int cleanupCounter = 0;
        if (++cleanupCounter >= 300) { // Clean up every ~5 seconds at 60 FPS
            cleanupInvalidEntities();
            cleanupCounter = 0;
        }

        m_lastFrameUpdates = 0;
        int updatesThisFrame = 0;
        int totalEntities = static_cast<int>(m_aiEntities.size());

        // Round-robin update to distribute load across frames
        for (int i = 0; i < totalEntities && updatesThisFrame < m_maxUpdatesPerFrame; ++i) {
            int entityIndex = (m_currentUpdateIndex + i) % totalEntities;
            
            if (auto entity = m_aiEntities[entityIndex].lock()) {
                if (auto aiComponent = entity->getComponent<AIComponent>()) {
                    aiComponent->update(deltaTime);
                    updatesThisFrame++;
                    m_lastFrameUpdates++;
                }
            }
        }

        // Update the starting index for next frame
        m_currentUpdateIndex = (m_currentUpdateIndex + updatesThisFrame) % std::max(1, totalEntities);

        if (m_debugMode && m_lastFrameUpdates > 0) {
            std::cout << "AI System: Updated " << m_lastFrameUpdates << " AI entities this frame" << std::endl;
        }
    }

    void AISystem::broadcastEvent(const AIEvent& event) {
        for (const auto& weak_entity : m_aiEntities) {
            if (auto entity = weak_entity.lock()) {
                if (auto aiComponent = entity->getComponent<AIComponent>()) {
                    aiComponent->processEvent(event);
                }
            }
        }
    }

    void AISystem::broadcastEventInRadius(const AIEvent& event, const glm::vec3& center, float radius) {
        float radiusSquared = radius * radius;
        
        for (const auto& weak_entity : m_aiEntities) {
            if (auto entity = weak_entity.lock()) {
                if (auto transform = entity->getComponent<TransformComponent>()) {
                    glm::vec3 entityPos = transform->getPosition();
                    float distanceSquared = glm::dot(entityPos - center, entityPos - center);
                    
                    if (distanceSquared <= radiusSquared) {
                        if (auto aiComponent = entity->getComponent<AIComponent>()) {
                            aiComponent->processEvent(event);
                        }
                    }
                }
            }
        }
    }

    void AISystem::setGlobalTarget(const std::string& aiTag, std::shared_ptr<Entity> target) {
        m_globalTargets[aiTag] = target;
        
        // Update all AI entities with this tag
        for (const auto& weak_entity : m_aiEntities) {
            if (auto entity = weak_entity.lock()) {
                // Check if entity has the specified tag (this would need a tag system)
                // For now, apply to all AI entities
                if (auto aiComponent = entity->getComponent<AIComponent>()) {
                    aiComponent->setTarget(target);
                }
            }
        }
    }

    void AISystem::clearGlobalTarget(const std::string& aiTag) {
        auto it = m_globalTargets.find(aiTag);
        if (it != m_globalTargets.end()) {
            m_globalTargets.erase(it);
            
            // Clear target from relevant AI entities
            for (const auto& weak_entity : m_aiEntities) {
                if (auto entity = weak_entity.lock()) {
                    if (auto aiComponent = entity->getComponent<AIComponent>()) {
                        aiComponent->setTarget(nullptr);
                    }
                }
            }
        }
    }

    std::shared_ptr<Entity> AISystem::findNearestEntity(const glm::vec3& position, float range, const std::string& tag) const {
        std::shared_ptr<Entity> nearest = nullptr;
        float nearestDistance = range;

        for (const auto& weak_entity : m_aiEntities) {
            if (auto entity = weak_entity.lock()) {
                if (auto transform = entity->getComponent<TransformComponent>()) {
                    float distance = glm::length(transform->getPosition() - position);
                    
                    if (distance < nearestDistance) {
                        // If tag filtering is needed, check tag here
                        nearest = entity;
                        nearestDistance = distance;
                    }
                }
            }
        }

        return nearest;
    }

    void AISystem::activateBehaviorForTag(const std::string& aiTag, const std::string& behaviorName) {
        for (const auto& weak_entity : m_aiEntities) {
            if (auto entity = weak_entity.lock()) {
                // Check if entity has the specified tag (tag system needed)
                if (auto aiComponent = entity->getComponent<AIComponent>()) {
                    if (aiComponent->hasBehavior(behaviorName)) {
                        aiComponent->setActiveBehavior(behaviorName);
                    }
                }
            }
        }
    }

    void AISystem::pauseAIWithTag(const std::string& aiTag) {
        for (const auto& weak_entity : m_aiEntities) {
            if (auto entity = weak_entity.lock()) {
                if (auto aiComponent = entity->getComponent<AIComponent>()) {
                    aiComponent->pause();
                }
            }
        }
    }

    void AISystem::resumeAIWithTag(const std::string& aiTag) {
        for (const auto& weak_entity : m_aiEntities) {
            if (auto entity = weak_entity.lock()) {
                if (auto aiComponent = entity->getComponent<AIComponent>()) {
                    aiComponent->resume();
                }
            }
        }
    }

    void AISystem::setGlobalUpdateFrequency(float frequency) {
        m_globalUpdateFrequency = frequency;
        
        // Update all registered AI components
        for (const auto& weak_entity : m_aiEntities) {
            if (auto entity = weak_entity.lock()) {
                if (auto aiComponent = entity->getComponent<AIComponent>()) {
                    aiComponent->setUpdateFrequency(frequency);
                }
            }
        }
    }

    size_t AISystem::getActiveAICount() const {
        size_t activeCount = 0;
        
        for (const auto& weak_entity : m_aiEntities) {
            if (auto entity = weak_entity.lock()) {
                if (auto aiComponent = entity->getComponent<AIComponent>()) {
                    if (aiComponent->isActive()) {
                        activeCount++;
                    }
                }
            }
        }
        
        return activeCount;
    }

    void AISystem::printStatistics() const {
        std::cout << "\n=== AI System Statistics ===" << std::endl;
        std::cout << "Registered AI Entities: " << getRegisteredAICount() << std::endl;
        std::cout << "Active AI Entities: " << getActiveAICount() << std::endl;
        std::cout << "Max Updates Per Frame: " << m_maxUpdatesPerFrame << std::endl;
        std::cout << "Global Update Frequency: " << m_globalUpdateFrequency << " Hz" << std::endl;
        std::cout << "Last Frame Updates: " << m_lastFrameUpdates << std::endl;
        std::cout << "Global Targets: " << m_globalTargets.size() << std::endl;
        std::cout << "Debug Mode: " << (m_debugMode ? "ON" : "OFF") << std::endl;
        std::cout << "===========================" << std::endl;
    }

    AISystem& AISystem::getInstance() {
        static AISystem instance;
        return instance;
    }

    // Private helper methods

    void AISystem::cleanupInvalidEntities() {
        auto it = std::remove_if(m_aiEntities.begin(), m_aiEntities.end(),
            [](const std::weak_ptr<Entity>& weak_entity) {
                return weak_entity.expired();
            });
        
        if (it != m_aiEntities.end()) {
            size_t removedCount = std::distance(it, m_aiEntities.end());
            m_aiEntities.erase(it, m_aiEntities.end());
            
            if (m_debugMode) {
                std::cout << "Cleaned up " << removedCount << " invalid AI entities" << std::endl;
            }
        }
    }

    std::vector<std::shared_ptr<Entity>> AISystem::getEntitiesWithTag(const std::string& tag) const {
        std::vector<std::shared_ptr<Entity>> result;
        
        for (const auto& weak_entity : m_aiEntities) {
            if (auto entity = weak_entity.lock()) {
                // Tag system would be implemented here
                result.push_back(entity);
            }
        }
        
        return result;
    }

    AIComponent* AISystem::getAIComponent(std::shared_ptr<Entity> entity) const {
        if (!entity) {
            return nullptr;
        }
        
        return entity->getComponent<AIComponent>().get();
    }

} // namespace IKore
