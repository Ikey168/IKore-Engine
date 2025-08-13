#pragma once

#include "Entity.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace IKore {

    /**
     * @brief Debug information for a single entity
     */
    struct EntityDebugInfo {
        EntityID entityID;
        std::string name;
        std::string type;
        glm::vec3 position;
        glm::vec3 rotation;
        glm::vec3 scale;
        bool isActive;
        bool isVisible;
        std::vector<std::string> componentDetails;
        float frameTime; // Last update time in milliseconds
    };

    /**
     * @brief Performance metrics for the debug system
     */
    struct DebugPerformanceMetrics {
        float debugOverheadMs;
        size_t totalEntities;
        size_t visibleEntities;
        float averageFrameTime;
        float minFrameTime;
        float maxFrameTime;
    };

    /**
     * @brief Entity Debugging System for real-time entity inspection
     * 
     * This system provides a debug overlay to inspect entities in real-time,
     * displaying position, rotation, and component details with minimal
     * performance impact.
     */
    class EntityDebugSystem {
    public:
        /**
         * @brief Get the singleton instance
         * @return Reference to the EntityDebugSystem instance
         */
        static EntityDebugSystem& getInstance();

        /**
         * @brief Toggle debug mode on/off
         */
        void toggleDebugMode();

        /**
         * @brief Enable debug mode
         */
        void enableDebugMode();

        /**
         * @brief Disable debug mode
         */
        void disableDebugMode();

        /**
         * @brief Check if debug mode is enabled
         * @return True if debug mode is active
         */
        bool isDebugModeEnabled() const { return m_debugEnabled; }

        /**
         * @brief Update the debug system (call once per frame)
         * @param deltaTime Time since last frame in seconds
         */
        void update(float deltaTime);

        /**
         * @brief Render the debug overlay
         * 
         * This should be called after all scene rendering is complete
         */
        void renderDebugOverlay();

        /**
         * @brief Set the debug overlay position on screen
         * @param x X position (0.0 = left, 1.0 = right)
         * @param y Y position (0.0 = top, 1.0 = bottom)
         */
        void setOverlayPosition(float x, float y);

        /**
         * @brief Set maximum number of entities to display
         * @param maxEntities Maximum entities to show (0 = unlimited)
         */
        void setMaxDisplayEntities(size_t maxEntities) { m_maxDisplayEntities = maxEntities; }

        /**
         * @brief Enable/disable performance monitoring
         * @param enabled Whether to track performance metrics
         */
        void setPerformanceMonitoring(bool enabled) { m_performanceMonitoring = enabled; }

        /**
         * @brief Set entity filter function
         * @param filter Function to filter which entities to display
         */
        void setEntityFilter(std::function<bool(const std::shared_ptr<Entity>&)> filter);

        /**
         * @brief Get current performance metrics
         * @return Debug system performance data
         */
        const DebugPerformanceMetrics& getPerformanceMetrics() const { return m_performanceMetrics; }

        /**
         * @brief Get debug information for all tracked entities
         * @return Vector of entity debug information
         */
        const std::vector<EntityDebugInfo>& getEntityDebugInfo() const { return m_entityDebugInfo; }

        /**
         * @brief Set debug text color
         * @param color RGBA color values (0.0-1.0)
         */
        void setDebugTextColor(const glm::vec4& color) { m_textColor = color; }

        /**
         * @brief Set debug background color
         * @param color RGBA color values (0.0-1.0)
         */
        void setDebugBackgroundColor(const glm::vec4& color) { m_backgroundColor = color; }

        /**
         * @brief Set debug text size
         * @param size Text size multiplier (1.0 = normal)
         */
        void setDebugTextSize(float size) { m_textSize = size; }

        /**
         * @brief Enable/disable detailed component information
         * @param enabled Whether to show detailed component data
         */
        void setDetailedComponentInfo(bool enabled) { m_showDetailedComponents = enabled; }

    private:
        EntityDebugSystem() = default;
        ~EntityDebugSystem() = default;
        EntityDebugSystem(const EntityDebugSystem&) = delete;
        EntityDebugSystem& operator=(const EntityDebugSystem&) = delete;

        // Core state
        bool m_debugEnabled = false;
        bool m_performanceMonitoring = true;
        bool m_showDetailedComponents = true;

        // Display settings
        glm::vec2 m_overlayPosition = glm::vec2(0.02f, 0.02f); // Top-left corner
        size_t m_maxDisplayEntities = 50; // Limit for performance
        glm::vec4 m_textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // White
        glm::vec4 m_backgroundColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.7f); // Semi-transparent black
        float m_textSize = 1.0f;

        // Debug data
        std::vector<EntityDebugInfo> m_entityDebugInfo;
        DebugPerformanceMetrics m_performanceMetrics;

        // Performance tracking
        float m_frameTimeAccumulator = 0.0f;
        float m_debugStartTime = 0.0f;
        size_t m_frameCount = 0;

        // Entity filtering
        std::function<bool(const std::shared_ptr<Entity>&)> m_entityFilter;

        /**
         * @brief Collect debug information from all entities
         * @param deltaTime Time since last frame
         */
        void collectEntityDebugInfo(float deltaTime);

        /**
         * @brief Extract debug information from a single entity
         * @param entity Entity to analyze
         * @return Debug information structure
         */
        EntityDebugInfo extractEntityInfo(const std::shared_ptr<Entity>& entity);

        /**
         * @brief Get detailed component information for an entity
         * @param entity Entity to analyze
         * @return Vector of component detail strings
         */
        std::vector<std::string> getComponentDetails(const std::shared_ptr<Entity>& entity);

        /**
         * @brief Update performance metrics
         * @param deltaTime Time since last frame
         * @param debugTime Time spent on debug operations
         */
        void updatePerformanceMetrics(float deltaTime, float debugTime);

        /**
         * @brief Render debug text to screen
         * @param text Text to display
         * @param x Screen X position (0.0-1.0)
         * @param y Screen Y position (0.0-1.0)
         * @param color Text color
         */
        void renderDebugText(const std::string& text, float x, float y, const glm::vec4& color);

        /**
         * @brief Render background panel for debug overlay
         * @param x Panel X position (0.0-1.0)
         * @param y Panel Y position (0.0-1.0)
         * @param width Panel width (0.0-1.0)
         * @param height Panel height (0.0-1.0)
         */
        void renderDebugBackground(float x, float y, float width, float height);

        /**
         * @brief Get current time in milliseconds
         * @return Current high-precision time
         */
        float getCurrentTimeMs() const;
    };

    /**
     * @brief Convenience function to get the debug system instance
     * @return Reference to the EntityDebugSystem
     */
    EntityDebugSystem& getEntityDebugSystem();

} // namespace IKore
