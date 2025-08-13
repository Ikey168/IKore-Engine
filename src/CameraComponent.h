#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <memory>
#include <functional>
#include "Entity.h"

namespace IKore {

    // Forward declarations
    class Entity;

    /**
     * @brief Camera Component for managing view and projection matrices
     * 
     * This component provides comprehensive camera functionality including:
     * - View and projection matrix management
     * - Multiple camera types (First-Person, Third-Person, Orbital)
     * - Entity following with smooth interpolation
     * - Smooth camera transitions between states
     * - Configurable projection parameters
     */
    class CameraComponent {
    public:
        /**
         * @brief Camera type enumeration
         */
        enum class CameraType {
            FIRST_PERSON,    ///< First-person camera (attached to entity)
            THIRD_PERSON,    ///< Third-person camera (follows entity from behind)
            ORBITAL,         ///< Orbital camera (orbits around target)
            FREE_CAMERA,     ///< Free-floating camera (manual control)
            STATIC           ///< Static camera (fixed position and orientation)
        };

        /**
         * @brief Projection type enumeration
         */
        enum class ProjectionType {
            PERSPECTIVE,     ///< Perspective projection
            ORTHOGRAPHIC     ///< Orthographic projection
        };

        /**
         * @brief Camera transition state for smooth transitions
         */
        struct TransitionState {
            bool isTransitioning = false;        ///< Whether transition is active
            float transitionTime = 0.0f;         ///< Current transition time
            float transitionDuration = 1.0f;     ///< Total transition duration
            glm::vec3 startPosition;              ///< Starting position
            glm::vec3 targetPosition;             ///< Target position
            glm::quat startRotation;              ///< Starting rotation
            glm::quat targetRotation;             ///< Target rotation
            CameraType startType;                 ///< Starting camera type
            CameraType targetType;                ///< Target camera type
        };

        /**
         * @brief Configuration for third-person cameras
         */
        struct ThirdPersonConfig {
            float distance = 5.0f;                ///< Distance from target
            float height = 2.0f;                  ///< Height offset from target
            float followSpeed = 5.0f;             ///< Speed of following movement
            float rotationSpeed = 3.0f;           ///< Speed of rotation following
            glm::vec3 offset = glm::vec3(0.0f);   ///< Additional offset from target
        };

        /**
         * @brief Configuration for orbital cameras
         */
        struct OrbitalConfig {
            float radius = 10.0f;                 ///< Orbital radius
            float azimuth = 0.0f;                 ///< Azimuth angle (horizontal)
            float elevation = 0.0f;               ///< Elevation angle (vertical)
            float orbitSpeed = 1.0f;              ///< Orbital movement speed
            bool autoOrbit = false;               ///< Automatic orbital movement
            float autoOrbitSpeed = 0.5f;          ///< Speed of automatic orbit
        };

        // ============================================================================
        // Constructors and Destructor
        // ============================================================================

        /**
         * @brief Default constructor
         */
        CameraComponent();

        /**
         * @brief Constructor with initial parameters
         * @param position Initial camera position
         * @param target Initial target/look-at position
         * @param up Initial up vector
         * @param fov Field of view in degrees (for perspective projection)
         * @param aspectRatio Aspect ratio (width/height)
         * @param nearPlane Near clipping plane
         * @param farPlane Far clipping plane
         */
        CameraComponent(const glm::vec3& position,
                       const glm::vec3& target = glm::vec3(0.0f),
                       const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f),
                       float fov = 45.0f,
                       float aspectRatio = 16.0f / 9.0f,
                       float nearPlane = 0.1f,
                       float farPlane = 100.0f);

        /**
         * @brief Copy constructor
         */
        CameraComponent(const CameraComponent& other);

        /**
         * @brief Move constructor
         */
        CameraComponent(CameraComponent&& other) noexcept;

        /**
         * @brief Copy assignment operator
         */
        CameraComponent& operator=(const CameraComponent& other);

        /**
         * @brief Move assignment operator
         */
        CameraComponent& operator=(CameraComponent&& other) noexcept;

        /**
         * @brief Destructor
         */
        ~CameraComponent() = default;

        // ============================================================================
        // Core Matrix Methods
        // ============================================================================

        /**
         * @brief Get the view matrix
         * @return Current view matrix
         */
        const glm::mat4& getViewMatrix() const;

        /**
         * @brief Get the projection matrix
         * @return Current projection matrix
         */
        const glm::mat4& getProjectionMatrix() const;

        /**
         * @brief Get the combined view-projection matrix
         * @return View-projection matrix
         */
        glm::mat4 getViewProjectionMatrix() const;

        /**
         * @brief Force recalculation of matrices
         */
        void updateMatrices();

        // ============================================================================
        // Camera Type and Configuration
        // ============================================================================

        /**
         * @brief Set camera type
         * @param type New camera type
         * @param transitionDuration Duration of transition in seconds (0 = immediate)
         */
        void setCameraType(CameraType type, float transitionDuration = 0.0f);

        /**
         * @brief Get current camera type
         * @return Current camera type
         */
        CameraType getCameraType() const { return m_currentType; }

        /**
         * @brief Set projection type
         * @param type Projection type
         */
        void setProjectionType(ProjectionType type);

        /**
         * @brief Get projection type
         * @return Current projection type
         */
        ProjectionType getProjectionType() const { return m_projectionType; }

        // ============================================================================
        // Entity Following
        // ============================================================================

        /**
         * @brief Set the entity to follow
         * @param entity Entity to follow (nullptr to clear)
         */
        void setFollowTarget(std::shared_ptr<Entity> entity);

        /**
         * @brief Get the entity being followed
         * @return Shared pointer to followed entity (may be null)
         */
        std::shared_ptr<Entity> getFollowTarget() const { return m_followTarget.lock(); }

        /**
         * @brief Check if camera is following an entity
         * @return True if following an entity
         */
        bool isFollowingEntity() const { return !m_followTarget.expired(); }

        // ============================================================================
        // Position and Orientation
        // ============================================================================

        /**
         * @brief Set camera position
         * @param position New position
         */
        void setPosition(const glm::vec3& position);

        /**
         * @brief Get camera position
         * @return Current camera position
         */
        const glm::vec3& getPosition() const { return m_position; }

        /**
         * @brief Set target position (what the camera looks at)
         * @param target Target position
         */
        void setTarget(const glm::vec3& target);

        /**
         * @brief Get target position
         * @return Current target position
         */
        const glm::vec3& getTarget() const { return m_target; }

        /**
         * @brief Set up vector
         * @param up New up vector
         */
        void setUp(const glm::vec3& up);

        /**
         * @brief Get up vector
         * @return Current up vector
         */
        const glm::vec3& getUp() const { return m_up; }

        /**
         * @brief Look at a specific target
         * @param target Target position
         * @param up Up vector (optional)
         */
        void lookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));

        // ============================================================================
        // Direction Vectors
        // ============================================================================

        /**
         * @brief Get forward direction vector
         * @return Normalized forward vector
         */
        glm::vec3 getForward() const;

        /**
         * @brief Get right direction vector
         * @return Normalized right vector
         */
        glm::vec3 getRight() const;

        /**
         * @brief Get actual up direction vector (may differ from set up vector)
         * @return Normalized up vector
         */
        glm::vec3 getActualUp() const;

        // ============================================================================
        // Projection Parameters
        // ============================================================================

        /**
         * @brief Set field of view (for perspective projection)
         * @param fov Field of view in degrees
         */
        void setFieldOfView(float fov);

        /**
         * @brief Get field of view
         * @return Field of view in degrees
         */
        float getFieldOfView() const { return m_fov; }

        /**
         * @brief Set aspect ratio
         * @param aspectRatio Aspect ratio (width/height)
         */
        void setAspectRatio(float aspectRatio);

        /**
         * @brief Get aspect ratio
         * @return Current aspect ratio
         */
        float getAspectRatio() const { return m_aspectRatio; }

        /**
         * @brief Set near clipping plane
         * @param nearPlane Near plane distance
         */
        void setNearPlane(float nearPlane);

        /**
         * @brief Get near clipping plane
         * @return Near plane distance
         */
        float getNearPlane() const { return m_nearPlane; }

        /**
         * @brief Set far clipping plane
         * @param farPlane Far plane distance
         */
        void setFarPlane(float farPlane);

        /**
         * @brief Get far clipping plane
         * @return Far plane distance
         */
        float getFarPlane() const { return m_farPlane; }

        /**
         * @brief Set orthographic size (for orthographic projection)
         * @param size Orthographic view size
         */
        void setOrthographicSize(float size);

        /**
         * @brief Get orthographic size
         * @return Orthographic view size
         */
        float getOrthographicSize() const { return m_orthographicSize; }

        // ============================================================================
        // Camera Type Configurations
        // ============================================================================

        /**
         * @brief Set third-person configuration
         * @param config Third-person configuration
         */
        void setThirdPersonConfig(const ThirdPersonConfig& config);

        /**
         * @brief Get third-person configuration
         * @return Current third-person configuration
         */
        const ThirdPersonConfig& getThirdPersonConfig() const { return m_thirdPersonConfig; }

        /**
         * @brief Set orbital configuration
         * @param config Orbital configuration
         */
        void setOrbitalConfig(const OrbitalConfig& config);

        /**
         * @brief Get orbital configuration
         * @return Current orbital configuration
         */
        const OrbitalConfig& getOrbitalConfig() const { return m_orbitalConfig; }

        // ============================================================================
        // Update and Animation
        // ============================================================================

        /**
         * @brief Update camera logic
         * @param deltaTime Time since last update
         */
        void update(float deltaTime);

        /**
         * @brief Check if camera is currently transitioning
         * @return True if transitioning between states
         */
        bool isTransitioning() const { return m_transitionState.isTransitioning; }

        /**
         * @brief Get transition progress (0.0 to 1.0)
         * @return Transition progress
         */
        float getTransitionProgress() const;

        // ============================================================================
        // Advanced Camera Control
        // ============================================================================

        /**
         * @brief Move camera relative to its current orientation
         * @param direction Movement direction in camera space
         * @param distance Movement distance
         */
        void moveRelative(const glm::vec3& direction, float distance);

        /**
         * @brief Rotate camera by given angles
         * @param pitch Pitch rotation in degrees
         * @param yaw Yaw rotation in degrees
         * @param roll Roll rotation in degrees
         */
        void rotate(float pitch, float yaw, float roll = 0.0f);

        /**
         * @brief Set camera rotation from Euler angles
         * @param pitch Pitch angle in degrees
         * @param yaw Yaw angle in degrees
         * @param roll Roll angle in degrees
         */
        void setRotation(float pitch, float yaw, float roll = 0.0f);

        /**
         * @brief Get camera rotation as Euler angles
         * @return Rotation as vec3 (pitch, yaw, roll) in degrees
         */
        glm::vec3 getRotation() const;

        /**
         * @brief Set camera rotation from quaternion
         * @param rotation Rotation quaternion
         */
        void setRotationQuaternion(const glm::quat& rotation);

        /**
         * @brief Get camera rotation as quaternion
         * @return Rotation quaternion
         */
        glm::quat getRotationQuaternion() const;

        // ============================================================================
        // Utility Methods
        // ============================================================================

        /**
         * @brief Convert screen coordinates to world ray
         * @param screenPos Screen position (normalized 0-1)
         * @param screenSize Screen size in pixels
         * @return Ray direction in world space
         */
        glm::vec3 screenToWorldRay(const glm::vec2& screenPos, const glm::vec2& screenSize) const;

        /**
         * @brief Convert world position to screen coordinates
         * @param worldPos World position
         * @param screenSize Screen size in pixels
         * @return Screen position in pixels
         */
        glm::vec2 worldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize) const;

        /**
         * @brief Check if position is within camera frustum
         * @param position World position to check
         * @param radius Bounding sphere radius (optional)
         * @return True if position is visible
         */
        bool isPositionVisible(const glm::vec3& position, float radius = 0.0f) const;

    private:
        // ============================================================================
        // Private Methods
        // ============================================================================

        /**
         * @brief Update view matrix
         */
        void updateViewMatrix();

        /**
         * @brief Update projection matrix
         */
        void updateProjectionMatrix();

        /**
         * @brief Update camera based on current type
         * @param deltaTime Time since last update
         */
        void updateCameraType(float deltaTime);

        /**
         * @brief Update first-person camera
         * @param deltaTime Time since last update
         */
        void updateFirstPerson(float deltaTime);

        /**
         * @brief Update third-person camera
         * @param deltaTime Time since last update
         */
        void updateThirdPerson(float deltaTime);

        /**
         * @brief Update orbital camera
         * @param deltaTime Time since last update
         */
        void updateOrbital(float deltaTime);

        /**
         * @brief Update camera transitions
         * @param deltaTime Time since last update
         */
        void updateTransition(float deltaTime);

        /**
         * @brief Calculate third-person camera position
         * @param targetPos Target entity position
         * @return Calculated camera position
         */
        glm::vec3 calculateThirdPersonPosition(const glm::vec3& targetPos) const;

        /**
         * @brief Calculate orbital camera position
         * @param centerPos Center/target position
         * @return Calculated camera position
         */
        glm::vec3 calculateOrbitalPosition(const glm::vec3& centerPos) const;

        /**
         * @brief Interpolate between two positions smoothly
         * @param start Start position
         * @param end End position
         * @param t Interpolation factor (0-1)
         * @return Interpolated position
         */
        glm::vec3 smoothInterpolate(const glm::vec3& start, const glm::vec3& end, float t) const;

        /**
         * @brief Mark matrices as needing update
         */
        void markDirty();

        // ============================================================================
        // Member Variables
        // ============================================================================

        // Core camera properties
        glm::vec3 m_position;           ///< Camera position
        glm::vec3 m_target;             ///< Target/look-at position
        glm::vec3 m_up;                 ///< Up vector

        // Matrices
        mutable glm::mat4 m_viewMatrix;        ///< View matrix
        mutable glm::mat4 m_projectionMatrix;  ///< Projection matrix
        mutable bool m_viewDirty;              ///< View matrix needs update
        mutable bool m_projectionDirty;        ///< Projection matrix needs update

        // Projection parameters
        ProjectionType m_projectionType;  ///< Projection type
        float m_fov;                     ///< Field of view (degrees)
        float m_aspectRatio;             ///< Aspect ratio
        float m_nearPlane;               ///< Near clipping plane
        float m_farPlane;                ///< Far clipping plane
        float m_orthographicSize;        ///< Orthographic size

        // Camera type and behavior
        CameraType m_currentType;        ///< Current camera type
        std::weak_ptr<Entity> m_followTarget;  ///< Entity to follow

        // Configuration for different camera types
        ThirdPersonConfig m_thirdPersonConfig;  ///< Third-person configuration
        OrbitalConfig m_orbitalConfig;          ///< Orbital configuration

        // Transition system
        TransitionState m_transitionState;      ///< Transition state

        // Cached rotation for performance
        mutable glm::quat m_cachedRotation;     ///< Cached rotation quaternion
        mutable bool m_rotationDirty;           ///< Rotation cache needs update
    };

} // namespace IKore
