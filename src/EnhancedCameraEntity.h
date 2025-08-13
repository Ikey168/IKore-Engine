#pragma once

#include "Entity.h"
#include "CameraComponent.h"
#include "TransformableEntities.h"
#include <memory>
#include <string>

namespace IKore {

    /**
     * @brief Enhanced Camera Entity with Camera Component
     * 
     * This camera entity combines the positioning capabilities of TransformableCamera
     * with the advanced camera features of CameraComponent to provide a complete
     * camera system supporting:
     * - Multiple camera types (First-Person, Third-Person, Orbital, etc.)
     * - Entity following with smooth transitions
     * - Automatic view and projection matrix management
     * - Smooth camera transitions between different states
     */
    class EnhancedCameraEntity : public Entity {
    public:
        /**
         * @brief Constructor with default settings
         * @param name Camera name
         * @param position Initial position
         * @param target Initial target/look-at position
         * @param cameraType Initial camera type
         */
        EnhancedCameraEntity(const std::string& name = "Camera",
                    const glm::vec3& position = glm::vec3(0.0f, 0.0f, 5.0f),
                    const glm::vec3& target = glm::vec3(0.0f, 0.0f, 0.0f),
                    CameraComponent::CameraType cameraType = CameraComponent::CameraType::FREE_CAMERA);

        /**
         * @brief Constructor with full parameters
         * @param name Camera name
         * @param position Initial position
         * @param target Initial target position
         * @param up Initial up vector
         * @param fov Field of view in degrees
         * @param aspectRatio Aspect ratio (width/height)
         * @param nearPlane Near clipping plane
         * @param farPlane Far clipping plane
         * @param cameraType Initial camera type
         */
        EnhancedCameraEntity(const std::string& name,
                    const glm::vec3& position,
                    const glm::vec3& target,
                    const glm::vec3& up,
                    float fov,
                    float aspectRatio,
                    float nearPlane,
                    float farPlane,
                    CameraComponent::CameraType cameraType = CameraComponent::CameraType::FREE_CAMERA);

        /**
         * @brief Virtual destructor
         */
        virtual ~EnhancedCameraEntity() = default;

        // ============================================================================
        // Entity Overrides
        // ============================================================================

        /**
         * @brief Initialize the camera entity
         */
        void initialize() override;

        /**
         * @brief Update the camera entity
         * @param deltaTime Time since last update
         */
        void update(float deltaTime) override;

        /**
         * @brief Clean up camera resources
         */
        void cleanup() override;

        /**
         * @brief Get entity position (override from Entity)
         * @return Entity position
         */
        glm::vec3 getPosition() const override;

        // ============================================================================
        // Camera Component Access
        // ============================================================================

        /**
         * @brief Get the camera component
         * @return Reference to camera component
         */
        CameraComponent& getCameraComponent() { return m_cameraComponent; }

        /**
         * @brief Get the camera component (const)
         * @return Const reference to camera component
         */
        const CameraComponent& getCameraComponent() const { return m_cameraComponent; }

        // ============================================================================
        // Camera Properties and Settings
        // ============================================================================

        /**
         * @brief Get camera name
         * @return Camera name
         */
        const std::string& getName() const { return m_name; }

        /**
         * @brief Set camera name
         * @param name New camera name
         */
        void setName(const std::string& name) { m_name = name; }

        /**
         * @brief Get camera active state
         * @return True if camera is active
         */
        bool isActive() const { return m_active; }

        /**
         * @brief Set camera active state
         * @param active Active state
         */
        void setActive(bool active) { m_active = active; }

        // ============================================================================
        // Matrix Access (Convenience Methods)
        // ============================================================================

        /**
         * @brief Get view matrix
         * @return View matrix
         */
        const glm::mat4& getViewMatrix() const { return m_cameraComponent.getViewMatrix(); }

        /**
         * @brief Get projection matrix
         * @return Projection matrix
         */
        const glm::mat4& getProjectionMatrix() const { return m_cameraComponent.getProjectionMatrix(); }

        /**
         * @brief Get view-projection matrix
         * @return Combined view-projection matrix
         */
        glm::mat4 getViewProjectionMatrix() const { return m_cameraComponent.getViewProjectionMatrix(); }

        // ============================================================================
        // Camera Type and Behavior (Convenience Methods)
        // ============================================================================

        /**
         * @brief Set camera type with optional transition
         * @param type New camera type
         * @param transitionDuration Duration of transition in seconds (0 = immediate)
         */
        void setCameraType(CameraComponent::CameraType type, float transitionDuration = 0.0f) {
            m_cameraComponent.setCameraType(type, transitionDuration);
        }

        /**
         * @brief Get current camera type
         * @return Current camera type
         */
        CameraComponent::CameraType getCameraType() const { 
            return m_cameraComponent.getCameraType(); 
        }

        /**
         * @brief Set entity to follow
         * @param entity Entity to follow (nullptr to clear)
         */
        void setFollowTarget(std::shared_ptr<Entity> entity) {
            m_cameraComponent.setFollowTarget(entity);
        }

        /**
         * @brief Get entity being followed
         * @return Shared pointer to followed entity
         */
        std::shared_ptr<Entity> getFollowTarget() const {
            return m_cameraComponent.getFollowTarget();
        }

        /**
         * @brief Check if camera is following an entity
         * @return True if following an entity
         */
        bool isFollowingEntity() const {
            return m_cameraComponent.isFollowingEntity();
        }

        // ============================================================================
        // Position and Orientation (Convenience Methods)
        // ============================================================================

        /**
         * @brief Set camera position
         * @param position New position
         */
        void setPosition(const glm::vec3& position) {
            m_cameraComponent.setPosition(position);
        }

        /**
         * @brief Set target position
         * @param target Target position
         */
        void setTarget(const glm::vec3& target) {
            m_cameraComponent.setTarget(target);
        }

        /**
         * @brief Get target position
         * @return Target position
         */
        const glm::vec3& getTarget() const {
            return m_cameraComponent.getTarget();
        }

        /**
         * @brief Look at a target
         * @param target Target position
         * @param up Up vector
         */
        void lookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f)) {
            m_cameraComponent.lookAt(target, up);
        }

        // ============================================================================
        // Direction Vectors (Convenience Methods)
        // ============================================================================

        /**
         * @brief Get forward direction
         * @return Forward direction vector
         */
        glm::vec3 getForward() const { return m_cameraComponent.getForward(); }

        /**
         * @brief Get right direction
         * @return Right direction vector
         */
        glm::vec3 getRight() const { return m_cameraComponent.getRight(); }

        /**
         * @brief Get up direction
         * @return Up direction vector
         */
        glm::vec3 getUp() const { return m_cameraComponent.getActualUp(); }

        // ============================================================================
        // Projection Parameters (Convenience Methods)
        // ============================================================================

        /**
         * @brief Set field of view
         * @param fov Field of view in degrees
         */
        void setFieldOfView(float fov) { m_cameraComponent.setFieldOfView(fov); }

        /**
         * @brief Get field of view
         * @return Field of view in degrees
         */
        float getFieldOfView() const { return m_cameraComponent.getFieldOfView(); }

        /**
         * @brief Set aspect ratio
         * @param aspectRatio Aspect ratio (width/height)
         */
        void setAspectRatio(float aspectRatio) { m_cameraComponent.setAspectRatio(aspectRatio); }

        /**
         * @brief Get aspect ratio
         * @return Aspect ratio
         */
        float getAspectRatio() const { return m_cameraComponent.getAspectRatio(); }

        /**
         * @brief Set near clipping plane
         * @param nearPlane Near plane distance
         */
        void setNearPlane(float nearPlane) { m_cameraComponent.setNearPlane(nearPlane); }

        /**
         * @brief Get near clipping plane
         * @return Near plane distance
         */
        float getNearPlane() const { return m_cameraComponent.getNearPlane(); }

        /**
         * @brief Set far clipping plane
         * @param farPlane Far plane distance
         */
        void setFarPlane(float farPlane) { m_cameraComponent.setFarPlane(farPlane); }

        /**
         * @brief Get far clipping plane
         * @return Far plane distance
         */
        float getFarPlane() const { return m_cameraComponent.getFarPlane(); }

        // ============================================================================
        // Camera Control (Convenience Methods)
        // ============================================================================

        /**
         * @brief Move camera relative to its orientation
         * @param direction Movement direction in camera space
         * @param distance Movement distance
         */
        void moveRelative(const glm::vec3& direction, float distance) {
            m_cameraComponent.moveRelative(direction, distance);
        }

        /**
         * @brief Rotate camera
         * @param pitch Pitch rotation in degrees
         * @param yaw Yaw rotation in degrees
         * @param roll Roll rotation in degrees
         */
        void rotate(float pitch, float yaw, float roll = 0.0f) {
            m_cameraComponent.rotate(pitch, yaw, roll);
        }

        /**
         * @brief Set camera rotation
         * @param pitch Pitch angle in degrees
         * @param yaw Yaw angle in degrees
         * @param roll Roll angle in degrees
         */
        void setRotation(float pitch, float yaw, float roll = 0.0f) {
            m_cameraComponent.setRotation(pitch, yaw, roll);
        }

        /**
         * @brief Get camera rotation as Euler angles
         * @return Rotation as vec3 (pitch, yaw, roll) in degrees
         */
        glm::vec3 getRotation() const {
            return m_cameraComponent.getRotation();
        }

        // ============================================================================
        // Camera Type Configurations (Convenience Methods)
        // ============================================================================

        /**
         * @brief Set third-person camera configuration
         * @param config Third-person configuration
         */
        void setThirdPersonConfig(const CameraComponent::ThirdPersonConfig& config) {
            m_cameraComponent.setThirdPersonConfig(config);
        }

        /**
         * @brief Get third-person camera configuration
         * @return Third-person configuration
         */
        const CameraComponent::ThirdPersonConfig& getThirdPersonConfig() const {
            return m_cameraComponent.getThirdPersonConfig();
        }

        /**
         * @brief Set orbital camera configuration
         * @param config Orbital configuration
         */
        void setOrbitalConfig(const CameraComponent::OrbitalConfig& config) {
            m_cameraComponent.setOrbitalConfig(config);
        }

        /**
         * @brief Get orbital camera configuration
         * @return Orbital configuration
         */
        const CameraComponent::OrbitalConfig& getOrbitalConfig() const {
            return m_cameraComponent.getOrbitalConfig();
        }

        // ============================================================================
        // Transition and Animation (Convenience Methods)
        // ============================================================================

        /**
         * @brief Check if camera is transitioning
         * @return True if transitioning
         */
        bool isTransitioning() const {
            return m_cameraComponent.isTransitioning();
        }

        /**
         * @brief Get transition progress
         * @return Transition progress (0.0 to 1.0)
         */
        float getTransitionProgress() const {
            return m_cameraComponent.getTransitionProgress();
        }

        // ============================================================================
        // Utility Methods (Convenience Methods)
        // ============================================================================

        /**
         * @brief Convert screen coordinates to world ray
         * @param screenPos Screen position (normalized 0-1)
         * @param screenSize Screen size in pixels
         * @return Ray direction in world space
         */
        glm::vec3 screenToWorldRay(const glm::vec2& screenPos, const glm::vec2& screenSize) const {
            return m_cameraComponent.screenToWorldRay(screenPos, screenSize);
        }

        /**
         * @brief Convert world position to screen coordinates
         * @param worldPos World position
         * @param screenSize Screen size in pixels
         * @return Screen position in pixels
         */
        glm::vec2 worldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize) const {
            return m_cameraComponent.worldToScreen(worldPos, screenSize);
        }

        /**
         * @brief Check if position is visible to camera
         * @param position World position
         * @param radius Bounding sphere radius
         * @return True if visible
         */
        bool isPositionVisible(const glm::vec3& position, float radius = 0.0f) const {
            return m_cameraComponent.isPositionVisible(position, radius);
        }

        // ============================================================================
        // Debug and Information
        // ============================================================================

        /**
         * @brief Get string representation of camera state
         * @return Camera state information
         */
        std::string getDebugInfo() const;

    private:
        // ============================================================================
        // Member Variables
        // ============================================================================
        
        std::string m_name;              ///< Camera name
        CameraComponent m_cameraComponent; ///< Core camera component
        bool m_active;                   ///< Active state
    };

} // namespace IKore
