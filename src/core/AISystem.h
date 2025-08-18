#pragma once

#include "core/components/AIComponent.h"
#include "core/Entity.h"
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <string>

namespace IKore {

    /**
     * @brief AI System for managing AI components efficiently
     * 
     * This system handles:
     * - Batch processing of AI components for performance
     * - Event distribution to AI entities
     * - Target assignment and tracking
     * - Global AI behavior coordination
     * - Performance monitoring and optimization
     */
    class AISystem {
    public:
        AISystem();
        ~AISystem();

        // Entity management
        void registerAIEntity(std::shared_ptr<Entity> entity);
        void unregisterAIEntity(std::shared_ptr<Entity> entity);
        void updateAll(float deltaTime);

        // Event broadcasting
        void broadcastEvent(const AIEvent& event);
        void broadcastEventInRadius(const AIEvent& event, const glm::vec3& center, float radius);

        // Target management
        void setGlobalTarget(const std::string& aiTag, std::shared_ptr<Entity> target);
        void clearGlobalTarget(const std::string& aiTag);
        std::shared_ptr<Entity> findNearestEntity(const glm::vec3& position, float range, const std::string& tag = "") const;

        // Behavior coordination
        void activateBehaviorForTag(const std::string& aiTag, const std::string& behaviorName);
        void pauseAIWithTag(const std::string& aiTag);
        void resumeAIWithTag(const std::string& aiTag);

        // Performance management
        void setMaxAIUpdatesPerFrame(int maxUpdates) { m_maxUpdatesPerFrame = maxUpdates; }
        int getMaxAIUpdatesPerFrame() const { return m_maxUpdatesPerFrame; }
        void setGlobalUpdateFrequency(float frequency);

        // Debug and statistics
        size_t getRegisteredAICount() const { return m_aiEntities.size(); }
        size_t getActiveAICount() const;
        void printStatistics() const;
        void setDebugMode(bool enabled) { m_debugMode = enabled; }

        // Singleton access
        static AISystem& getInstance();

    private:
        std::vector<std::weak_ptr<Entity>> m_aiEntities;
        std::unordered_map<std::string, std::weak_ptr<Entity>> m_globalTargets;
        std::unordered_set<std::string> m_aiTags;
        
        // Performance optimization
        int m_maxUpdatesPerFrame{50};       // Limit AI updates per frame
        int m_currentUpdateIndex{0};        // Round-robin update index
        float m_globalUpdateFrequency{30.0f}; // Default global AI update frequency
        
        // Debug
        bool m_debugMode{false};
        mutable size_t m_lastFrameUpdates{0};

        // Helper methods
        void cleanupInvalidEntities();
        std::vector<std::shared_ptr<Entity>> getEntitiesWithTag(const std::string& tag) const;
        AIComponent* getAIComponent(std::shared_ptr<Entity> entity) const;
    };

} // namespace IKore
