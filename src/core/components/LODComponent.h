#pragma once

#include "../Component.h"
#include "../../Model.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>

namespace IKore {

    /**
     * @brief Level of Detail (LOD) levels for rendering optimization
     */
    enum class LODLevel {
        LOD_0 = 0,  // Highest detail (closest)
        LOD_1 = 1,  // High detail
        LOD_2 = 2,  // Medium detail
        LOD_3 = 3,  // Low detail
        LOD_4 = 4   // Lowest detail (furthest)
    };

    /**
     * @brief LOD configuration for distance-based switching
     */
    struct LODConfig {
        float distanceThreshold;    // Distance at which this LOD becomes active
        std::string modelPath;      // Path to the model for this LOD level
        int triangleCount;          // Approximate triangle count for this LOD
        bool enabled;               // Whether this LOD level is enabled
        
        LODConfig() : distanceThreshold(0.0f), triangleCount(0), enabled(false) {}
        LODConfig(float distance, const std::string& path, int triangles = 0) 
            : distanceThreshold(distance), modelPath(path), triangleCount(triangles), enabled(true) {}
    };

    /**
     * @brief Component for managing Level of Detail (LOD) rendering
     * 
     * This component automatically switches between different detail levels of a model
     * based on distance from the camera to optimize rendering performance.
     * 
     * Features:
     * - Distance-based LOD switching
     * - Smooth transitions between LOD levels
     * - Configurable distance thresholds
     * - Support for up to 5 LOD levels
     * - Performance metrics and monitoring
     * 
     * Usage:
     * @code
     * auto entity = std::make_shared<Entity>();
     * auto lodComp = entity->addComponent<LODComponent>();
     * 
     * // Add LOD levels
     * lodComp->addLODLevel(LODLevel::LOD_0, 0.0f, "models/character_high.obj");
     * lodComp->addLODLevel(LODLevel::LOD_1, 50.0f, "models/character_medium.obj");
     * lodComp->addLODLevel(LODLevel::LOD_2, 100.0f, "models/character_low.obj");
     * 
     * // Initialize and enable
     * lodComp->initialize();
     * lodComp->setEnabled(true);
     * @endcode
     */
    class LODComponent : public Component {
    public:
        /**
         * @brief Default constructor
         */
        LODComponent();
        
        /**
         * @brief Destructor
         */
        virtual ~LODComponent();
        
        // Component lifecycle
        void onAttach() override;
        void onDetach() override;
        
        /**
         * @brief Initialize the LOD component
         * @return True if initialization succeeded
         */
        bool initialize();
        
        /**
         * @brief Update LOD based on camera distance
         * @param cameraPosition Current camera position
         * @param deltaTime Time elapsed since last update
         */
        void update(const glm::vec3& cameraPosition, float deltaTime);
        
        /**
         * @brief Add a LOD level configuration
         * @param level LOD level enum
         * @param distanceThreshold Distance at which this LOD becomes active
         * @param modelPath Path to the model file
         * @param triangleCount Optional triangle count for performance tracking
         */
        void addLODLevel(LODLevel level, float distanceThreshold, const std::string& modelPath, int triangleCount = 0);
        
        /**
         * @brief Remove a LOD level
         * @param level LOD level to remove
         */
        void removeLODLevel(LODLevel level);
        
        /**
         * @brief Get the current active LOD level
         * @return Current LOD level
         */
        LODLevel getCurrentLODLevel() const { return m_currentLOD; }
        
        /**
         * @brief Get the current model for rendering
         * @return Pointer to current model or nullptr if none
         */
        std::shared_ptr<Model> getCurrentModel() const;
        
        /**
         * @brief Set enabled state
         * @param enabled Whether LOD system is enabled
         */
        void setEnabled(bool enabled) { m_enabled = enabled; }
        
        /**
         * @brief Check if LOD system is enabled
         * @return True if enabled
         */
        bool isEnabled() const { return m_enabled; }
        
        /**
         * @brief Set hysteresis factor to prevent LOD flickering
         * @param factor Hysteresis factor (0.0-1.0)
         */
        void setHysteresis(float factor);
        
        /**
         * @brief Get hysteresis factor
         * @return Current hysteresis factor
         */
        float getHysteresis() const { return m_hysteresis; }
        
        /**
         * @brief Set transition smoothing duration
         * @param duration Transition duration in seconds
         */
        void setTransitionDuration(float duration) { m_transitionDuration = duration; }
        
        /**
         * @brief Get transition duration
         * @return Current transition duration
         */
        float getTransitionDuration() const { return m_transitionDuration; }
        
        /**
         * @brief Force a specific LOD level (disables automatic switching)
         * @param level LOD level to force
         */
        void forceLODLevel(LODLevel level);
        
        /**
         * @brief Enable automatic LOD switching
         */
        void enableAutoLOD() { m_forcedLOD = false; }
        
        /**
         * @brief Check if LOD is in automatic mode
         * @return True if automatic LOD switching is enabled
         */
        bool isAutoLODEnabled() const { return !m_forcedLOD; }
        
        /**
         * @brief Get distance to camera
         * @return Current distance to camera
         */
        float getDistanceToCamera() const { return m_distanceToCamera; }
        
        /**
         * @brief Get LOD configuration for a specific level
         * @param level LOD level
         * @return LOD configuration
         */
        const LODConfig& getLODConfig(LODLevel level) const;
        
        /**
         * @brief Set LOD configuration for a specific level
         * @param level LOD level
         * @param config LOD configuration
         */
        void setLODConfig(LODLevel level, const LODConfig& config);
        
        /**
         * @brief Get performance statistics
         * @return String with performance info
         */
        std::string getPerformanceStats() const;
        
        /**
         * @brief Reset performance counters
         */
        void resetPerformanceStats();
        
        /**
         * @brief Get total triangle count for current LOD
         * @return Triangle count
         */
        int getCurrentTriangleCount() const;
        
        /**
         * @brief Check if a LOD level is available
         * @param level LOD level to check
         * @return True if level is configured and enabled
         */
        bool hasLODLevel(LODLevel level) const;

    private:
        // LOD state
        LODLevel m_currentLOD;
        LODLevel m_previousLOD;
        bool m_enabled;
        bool m_forcedLOD;
        bool m_initialized;
        
        // Distance calculation
        float m_distanceToCamera;
        float m_hysteresis;
        
        // Transition smoothing
        float m_transitionDuration;
        float m_transitionTimer;
        bool m_inTransition;
        
        // LOD configurations (indexed by LODLevel enum value)
        std::vector<LODConfig> m_lodConfigs;
        
        // Loaded models for each LOD level
        std::vector<std::shared_ptr<Model>> m_lodModels;
        
        // Performance tracking
        mutable int m_lodSwitchCount;
        mutable float m_totalUpdateTime;
        mutable int m_updateCount;
        
        /**
         * @brief Calculate distance to camera from entity position
         * @param cameraPosition Camera world position
         * @return Distance to camera
         */
        float calculateDistanceToCamera(const glm::vec3& cameraPosition) const;
        
        /**
         * @brief Determine appropriate LOD level based on distance
         * @param distance Distance to camera
         * @return Optimal LOD level
         */
        LODLevel determineLODLevel(float distance) const;
        
        /**
         * @brief Load model for a specific LOD level
         * @param level LOD level
         * @return True if model loaded successfully
         */
        bool loadLODModel(LODLevel level);
        
        /**
         * @brief Switch to a new LOD level
         * @param newLOD New LOD level to switch to
         */
        void switchToLOD(LODLevel newLOD);
        
        /**
         * @brief Update transition state
         * @param deltaTime Time elapsed since last update
         */
        void updateTransition(float deltaTime);
        
        /**
         * @brief Get entity position for distance calculation
         * @return World position of entity
         */
        glm::vec3 getEntityPosition() const;
    };

} // namespace IKore
