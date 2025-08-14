#include "CameraComponent.h"
#include "core/Logger.h"
#include <algorithm>
#include <cmath>

namespace IKore {

    // ============================================================================
    // Helper Functions
    // ============================================================================

    namespace {
        /**
         * @brief Smoothing function for camera interpolation
         */
        float smoothStep(float t) {
            return t * t * (3.0f - 2.0f * t);
        }

        /**
         * @brief Calculate quaternion from look direction
         */
        glm::quat lookRotation(const glm::vec3& forward, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f)) {
            glm::vec3 f = normalize(forward);
            glm::vec3 u = normalize(up);
            glm::vec3 r = normalize(cross(u, f));
            u = cross(f, r);

            glm::mat3 rotMatrix(r, u, f);
            return glm::quat_cast(rotMatrix);
        }

        /**
         * @brief Convert Euler angles to quaternion
         */
        glm::quat eulerToQuaternion(float pitch, float yaw, float roll) {
            glm::vec3 radians = glm::radians(glm::vec3(pitch, yaw, roll));
            return glm::quat(radians);
        }

        /**
         * @brief Convert quaternion to Euler angles
         */
        glm::vec3 quaternionToEuler(const glm::quat& q) {
            return glm::degrees(glm::eulerAngles(q));
        }
    }

    // ============================================================================
    // Constructors and Destructor
    // ============================================================================

    CameraComponent::CameraComponent()
        : m_position(0.0f, 0.0f, 5.0f)
        , m_target(0.0f, 0.0f, 0.0f)
        , m_up(0.0f, 1.0f, 0.0f)
        , m_viewMatrix(1.0f)
        , m_projectionMatrix(1.0f)
        , m_viewDirty(true)
        , m_projectionDirty(true)
        , m_projectionType(ProjectionType::PERSPECTIVE)
        , m_fov(45.0f)
        , m_aspectRatio(16.0f / 9.0f)
        , m_nearPlane(0.1f)
        , m_farPlane(100.0f)
        , m_orthographicSize(10.0f)
        , m_currentType(CameraType::FREE_CAMERA)
        , m_cachedRotation(1.0f, 0.0f, 0.0f, 0.0f)
        , m_rotationDirty(true) {
        
        LOG_INFO("CameraComponent created with default settings");
    }

    CameraComponent::CameraComponent(const glm::vec3& position,
                                   const glm::vec3& target,
                                   const glm::vec3& up,
                                   float fov,
                                   float aspectRatio,
                                   float nearPlane,
                                   float farPlane)
        : m_position(position)
        , m_target(target)
        , m_up(up)
        , m_viewMatrix(1.0f)
        , m_projectionMatrix(1.0f)
        , m_viewDirty(true)
        , m_projectionDirty(true)
        , m_projectionType(ProjectionType::PERSPECTIVE)
        , m_fov(fov)
        , m_aspectRatio(aspectRatio)
        , m_nearPlane(nearPlane)
        , m_farPlane(farPlane)
        , m_orthographicSize(10.0f)
        , m_currentType(CameraType::FREE_CAMERA)
        , m_cachedRotation(1.0f, 0.0f, 0.0f, 0.0f)
        , m_rotationDirty(true) {
        
        LOG_INFO("CameraComponent created with custom settings");
    }

    CameraComponent::CameraComponent(const CameraComponent& other)
        : m_position(other.m_position)
        , m_target(other.m_target)
        , m_up(other.m_up)
        , m_viewMatrix(other.m_viewMatrix)
        , m_projectionMatrix(other.m_projectionMatrix)
        , m_viewDirty(other.m_viewDirty)
        , m_projectionDirty(other.m_projectionDirty)
        , m_projectionType(other.m_projectionType)
        , m_fov(other.m_fov)
        , m_aspectRatio(other.m_aspectRatio)
        , m_nearPlane(other.m_nearPlane)
        , m_farPlane(other.m_farPlane)
        , m_orthographicSize(other.m_orthographicSize)
        , m_currentType(other.m_currentType)
        , m_followTarget(other.m_followTarget)
        , m_thirdPersonConfig(other.m_thirdPersonConfig)
        , m_orbitalConfig(other.m_orbitalConfig)
        , m_transitionState(other.m_transitionState)
        , m_cachedRotation(other.m_cachedRotation)
        , m_rotationDirty(other.m_rotationDirty) {
    }

    CameraComponent::CameraComponent(CameraComponent&& other) noexcept
        : m_position(std::move(other.m_position))
        , m_target(std::move(other.m_target))
        , m_up(std::move(other.m_up))
        , m_viewMatrix(std::move(other.m_viewMatrix))
        , m_projectionMatrix(std::move(other.m_projectionMatrix))
        , m_viewDirty(other.m_viewDirty)
        , m_projectionDirty(other.m_projectionDirty)
        , m_projectionType(other.m_projectionType)
        , m_fov(other.m_fov)
        , m_aspectRatio(other.m_aspectRatio)
        , m_nearPlane(other.m_nearPlane)
        , m_farPlane(other.m_farPlane)
        , m_orthographicSize(other.m_orthographicSize)
        , m_currentType(other.m_currentType)
        , m_followTarget(std::move(other.m_followTarget))
        , m_thirdPersonConfig(std::move(other.m_thirdPersonConfig))
        , m_orbitalConfig(std::move(other.m_orbitalConfig))
        , m_transitionState(std::move(other.m_transitionState))
        , m_cachedRotation(other.m_cachedRotation)
        , m_rotationDirty(other.m_rotationDirty) {
    }

    CameraComponent& CameraComponent::operator=(const CameraComponent& other) {
        if (this != &other) {
            m_position = other.m_position;
            m_target = other.m_target;
            m_up = other.m_up;
            m_viewMatrix = other.m_viewMatrix;
            m_projectionMatrix = other.m_projectionMatrix;
            m_viewDirty = other.m_viewDirty;
            m_projectionDirty = other.m_projectionDirty;
            m_projectionType = other.m_projectionType;
            m_fov = other.m_fov;
            m_aspectRatio = other.m_aspectRatio;
            m_nearPlane = other.m_nearPlane;
            m_farPlane = other.m_farPlane;
            m_orthographicSize = other.m_orthographicSize;
            m_currentType = other.m_currentType;
            m_followTarget = other.m_followTarget;
            m_thirdPersonConfig = other.m_thirdPersonConfig;
            m_orbitalConfig = other.m_orbitalConfig;
            m_transitionState = other.m_transitionState;
            m_cachedRotation = other.m_cachedRotation;
            m_rotationDirty = other.m_rotationDirty;
        }
        return *this;
    }

    CameraComponent& CameraComponent::operator=(CameraComponent&& other) noexcept {
        if (this != &other) {
            m_position = std::move(other.m_position);
            m_target = std::move(other.m_target);
            m_up = std::move(other.m_up);
            m_viewMatrix = std::move(other.m_viewMatrix);
            m_projectionMatrix = std::move(other.m_projectionMatrix);
            m_viewDirty = other.m_viewDirty;
            m_projectionDirty = other.m_projectionDirty;
            m_projectionType = other.m_projectionType;
            m_fov = other.m_fov;
            m_aspectRatio = other.m_aspectRatio;
            m_nearPlane = other.m_nearPlane;
            m_farPlane = other.m_farPlane;
            m_orthographicSize = other.m_orthographicSize;
            m_currentType = other.m_currentType;
            m_followTarget = std::move(other.m_followTarget);
            m_thirdPersonConfig = std::move(other.m_thirdPersonConfig);
            m_orbitalConfig = std::move(other.m_orbitalConfig);
            m_transitionState = std::move(other.m_transitionState);
            m_cachedRotation = other.m_cachedRotation;
            m_rotationDirty = other.m_rotationDirty;
        }
        return *this;
    }

    // ============================================================================
    // Core Matrix Methods
    // ============================================================================

    const glm::mat4& CameraComponent::getViewMatrix() const {
        if (m_viewDirty) {
            const_cast<CameraComponent*>(this)->updateViewMatrix();
        }
        return m_viewMatrix;
    }

    const glm::mat4& CameraComponent::getProjectionMatrix() const {
        if (m_projectionDirty) {
            const_cast<CameraComponent*>(this)->updateProjectionMatrix();
        }
        return m_projectionMatrix;
    }

    glm::mat4 CameraComponent::getViewProjectionMatrix() const {
        return getProjectionMatrix() * getViewMatrix();
    }

    void CameraComponent::updateMatrices() {
        updateViewMatrix();
        updateProjectionMatrix();
    }

    // ============================================================================
    // Camera Type and Configuration
    // ============================================================================

    void CameraComponent::setCameraType(CameraType type, float transitionDuration) {
        if (m_currentType == type && !m_transitionState.isTransitioning) {
            return;
        }

        if (transitionDuration > 0.0f) {
            // Start smooth transition
            m_transitionState.isTransitioning = true;
            m_transitionState.transitionTime = 0.0f;
            m_transitionState.transitionDuration = transitionDuration;
            m_transitionState.startPosition = m_position;
            m_transitionState.startRotation = getRotationQuaternion();
            m_transitionState.startType = m_currentType;
            m_transitionState.targetType = type;

            LOG_INFO("Starting camera transition from type " + std::to_string(static_cast<int>(m_currentType)) +
                     " to " + std::to_string(static_cast<int>(type)) +
                     " over " + std::to_string(transitionDuration) + " seconds");
        } else {
            // Immediate change
            m_currentType = type;
            markDirty();
            LOG_INFO("Camera type changed to " + std::to_string(static_cast<int>(type)));
        }
    }

    void CameraComponent::setProjectionType(ProjectionType type) {
        if (m_projectionType != type) {
            m_projectionType = type;
            m_projectionDirty = true;
            LOG_INFO(std::string("Camera projection type changed to ") + 
                     (type == ProjectionType::PERSPECTIVE ? "perspective" : "orthographic"));
        }
    }

    // ============================================================================
    // Entity Following
    // ============================================================================

    void CameraComponent::setFollowTarget(std::shared_ptr<Entity> entity) {
        m_followTarget = entity;
        if (entity) {
            LOG_INFO("Camera now following entity ID: " + std::to_string(entity->getID()));
        } else {
            LOG_INFO("Camera follow target cleared");
        }
    }

    // ============================================================================
    // Position and Orientation
    // ============================================================================

    void CameraComponent::setPosition(const glm::vec3& position) {
        if (m_position != position) {
            m_position = position;
            markDirty();
        }
    }

    void CameraComponent::setTarget(const glm::vec3& target) {
        if (m_target != target) {
            m_target = target;
            markDirty();
        }
    }

    void CameraComponent::setUp(const glm::vec3& up) {
        if (m_up != up) {
            m_up = normalize(up);
            markDirty();
        }
    }

    void CameraComponent::lookAt(const glm::vec3& target, const glm::vec3& up) {
        setTarget(target);
        setUp(up);
        markDirty();
    }

    // ============================================================================
    // Direction Vectors
    // ============================================================================

    glm::vec3 CameraComponent::getForward() const {
        return normalize(m_target - m_position);
    }

    glm::vec3 CameraComponent::getRight() const {
        return normalize(cross(getForward(), m_up));
    }

    glm::vec3 CameraComponent::getActualUp() const {
        return normalize(cross(getRight(), getForward()));
    }

    // ============================================================================
    // Projection Parameters
    // ============================================================================

    void CameraComponent::setFieldOfView(float fov) {
        fov = std::clamp(fov, 1.0f, 179.0f);
        if (m_fov != fov) {
            m_fov = fov;
            m_projectionDirty = true;
        }
    }

    void CameraComponent::setAspectRatio(float aspectRatio) {
        aspectRatio = std::max(aspectRatio, 0.01f);
        if (m_aspectRatio != aspectRatio) {
            m_aspectRatio = aspectRatio;
            m_projectionDirty = true;
        }
    }

    void CameraComponent::setNearPlane(float nearPlane) {
        nearPlane = std::max(nearPlane, 0.001f);
        if (m_nearPlane != nearPlane) {
            m_nearPlane = nearPlane;
            m_projectionDirty = true;
        }
    }

    void CameraComponent::setFarPlane(float farPlane) {
        farPlane = std::max(farPlane, m_nearPlane + 0.001f);
        if (m_farPlane != farPlane) {
            m_farPlane = farPlane;
            m_projectionDirty = true;
        }
    }

    void CameraComponent::setOrthographicSize(float size) {
        size = std::max(size, 0.01f);
        if (m_orthographicSize != size) {
            m_orthographicSize = size;
            m_projectionDirty = true;
        }
    }

    // ============================================================================
    // Camera Type Configurations
    // ============================================================================

    void CameraComponent::setThirdPersonConfig(const ThirdPersonConfig& config) {
        m_thirdPersonConfig = config;
        LOG_INFO("Third-person camera configuration updated");
    }

    void CameraComponent::setOrbitalConfig(const OrbitalConfig& config) {
        m_orbitalConfig = config;
        LOG_INFO("Orbital camera configuration updated");
    }

    // ============================================================================
    // Update and Animation
    // ============================================================================

    void CameraComponent::update(float deltaTime) {
        // Update transitions first
        if (m_transitionState.isTransitioning) {
            updateTransition(deltaTime);
        }

        // Update camera based on current type
        updateCameraType(deltaTime);

        // Update matrices if needed
        if (m_viewDirty) {
            updateViewMatrix();
        }
        if (m_projectionDirty) {
            updateProjectionMatrix();
        }
    }

    float CameraComponent::getTransitionProgress() const {
        if (!m_transitionState.isTransitioning) {
            return 1.0f;
        }
        return std::clamp(m_transitionState.transitionTime / m_transitionState.transitionDuration, 0.0f, 1.0f);
    }

    // ============================================================================
    // Advanced Camera Control
    // ============================================================================

    void CameraComponent::moveRelative(const glm::vec3& direction, float distance) {
        glm::vec3 forward = getForward();
        glm::vec3 right = getRight();
        glm::vec3 up = getActualUp();

        glm::vec3 movement = (forward * direction.z + right * direction.x + up * direction.y) * distance;
        setPosition(m_position + movement);
    }

    void CameraComponent::rotate(float pitch, float yaw, float roll) {
        glm::vec3 currentRotation = getRotation();
        setRotation(currentRotation.x + pitch, currentRotation.y + yaw, currentRotation.z + roll);
    }

    void CameraComponent::setRotation(float pitch, float yaw, float roll) {
        pitch = std::clamp(pitch, -89.0f, 89.0f);
        
        glm::quat rotation = eulerToQuaternion(pitch, yaw, roll);
        setRotationQuaternion(rotation);
    }

    glm::vec3 CameraComponent::getRotation() const {
        if (m_rotationDirty) {
            glm::vec3 forward = getForward();
            const_cast<CameraComponent*>(this)->m_cachedRotation = lookRotation(forward, m_up);
            const_cast<CameraComponent*>(this)->m_rotationDirty = false;
        }
        return quaternionToEuler(m_cachedRotation);
    }

    void CameraComponent::setRotationQuaternion(const glm::quat& rotation) {
        m_cachedRotation = normalize(rotation);
        m_rotationDirty = false;

        // Update target based on rotation
        glm::vec3 forward = m_cachedRotation * glm::vec3(0.0f, 0.0f, -1.0f);
        setTarget(m_position + forward);
    }

    glm::quat CameraComponent::getRotationQuaternion() const {
        if (m_rotationDirty) {
            glm::vec3 forward = getForward();
            const_cast<CameraComponent*>(this)->m_cachedRotation = lookRotation(forward, m_up);
            const_cast<CameraComponent*>(this)->m_rotationDirty = false;
        }
        return m_cachedRotation;
    }

    // ============================================================================
    // Utility Methods
    // ============================================================================

    glm::vec3 CameraComponent::screenToWorldRay(const glm::vec2& screenPos, const glm::vec2& screenSize) const {
        // Convert screen coordinates to normalized device coordinates
        glm::vec2 ndc = (screenPos / screenSize) * 2.0f - 1.0f;
        ndc.y = -ndc.y; // Flip Y coordinate

        // Create ray in clip space
        glm::vec4 rayClip(ndc.x, ndc.y, -1.0f, 1.0f);

        // Transform to eye space
        glm::mat4 invProjection = inverse(getProjectionMatrix());
        glm::vec4 rayEye = invProjection * rayClip;
        rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

        // Transform to world space
        glm::mat4 invView = inverse(getViewMatrix());
        glm::vec4 rayWorld = invView * rayEye;

        return normalize(glm::vec3(rayWorld));
    }

    glm::vec2 CameraComponent::worldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize) const {
        glm::mat4 mvp = getViewProjectionMatrix();
        glm::vec4 clipPos = mvp * glm::vec4(worldPos, 1.0f);

        if (clipPos.w == 0.0f) {
            return glm::vec2(-1.0f); // Invalid position
        }

        // Perspective divide
        glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;

        // Convert to screen coordinates
        glm::vec2 screenPos;
        screenPos.x = (ndcPos.x + 1.0f) * 0.5f * screenSize.x;
        screenPos.y = (1.0f - ndcPos.y) * 0.5f * screenSize.y; // Flip Y

        return screenPos;
    }

    bool CameraComponent::isPositionVisible(const glm::vec3& position, float radius) const {
        // Simple frustum check - could be improved with proper frustum culling
        glm::mat4 mvp = getViewProjectionMatrix();
        glm::vec4 clipPos = mvp * glm::vec4(position, 1.0f);

        if (clipPos.w <= 0.0f) {
            return false; // Behind camera
        }

        glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;
        
        // Check if within NDC bounds (with radius consideration)
        float radiusNDC = radius / clipPos.w; // Approximate radius in NDC space
        return (ndcPos.x >= -1.0f - radiusNDC && ndcPos.x <= 1.0f + radiusNDC &&
                ndcPos.y >= -1.0f - radiusNDC && ndcPos.y <= 1.0f + radiusNDC &&
                ndcPos.z >= -1.0f && ndcPos.z <= 1.0f);
    }

    // ============================================================================
    // Private Methods
    // ============================================================================

    void CameraComponent::updateViewMatrix() {
        m_viewMatrix = glm::lookAt(m_position, m_target, m_up);
        m_viewDirty = false;
    }

    void CameraComponent::updateProjectionMatrix() {
        if (m_projectionType == ProjectionType::PERSPECTIVE) {
            m_projectionMatrix = glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearPlane, m_farPlane);
        } else {
            float halfSize = m_orthographicSize * 0.5f;
            float left = -halfSize * m_aspectRatio;
            float right = halfSize * m_aspectRatio;
            float bottom = -halfSize;
            float top = halfSize;
            m_projectionMatrix = glm::ortho(left, right, bottom, top, m_nearPlane, m_farPlane);
        }
        m_projectionDirty = false;
    }

    void CameraComponent::updateCameraType(float deltaTime) {
        switch (m_currentType) {
            case CameraType::FIRST_PERSON:
                updateFirstPerson(deltaTime);
                break;
            case CameraType::THIRD_PERSON:
                updateThirdPerson(deltaTime);
                break;
            case CameraType::ORBITAL:
                updateOrbital(deltaTime);
                break;
            case CameraType::FREE_CAMERA:
            case CameraType::STATIC:
                // No automatic updates for free/static cameras
                break;
        }
    }

    void CameraComponent::updateFirstPerson(float deltaTime) {
        auto target = m_followTarget.lock();
        if (!target) {
            return;
        }

        // For first-person cameras, position the camera at the entity's position
        // The target should be set based on the entity's forward direction
        // This requires the entity to have position and rotation information
        // For now, we'll implement a basic follow system
        
        // Note: This assumes the target entity has a position we can access
        // In a real implementation, you'd need to access the entity's transform
        
        setPosition(target->getPosition()); // This assumes Entity has getPosition()
    }

    void CameraComponent::updateThirdPerson(float deltaTime) {
        auto target = m_followTarget.lock();
        if (!target) {
            return;
        }

        glm::vec3 targetPos = target->getPosition(); // This assumes Entity has getPosition()
        glm::vec3 desiredPosition = calculateThirdPersonPosition(targetPos);

        // Smooth interpolation to desired position
        float lerpSpeed = m_thirdPersonConfig.followSpeed * deltaTime;
        glm::vec3 newPosition = smoothInterpolate(m_position, desiredPosition, lerpSpeed);
        
        setPosition(newPosition);
        setTarget(targetPos + m_thirdPersonConfig.offset);
    }

    void CameraComponent::updateOrbital(float deltaTime) {
        auto target = m_followTarget.lock();
        glm::vec3 centerPos = target ? target->getPosition() : m_target;

        // Update orbital angles if auto-orbit is enabled
        if (m_orbitalConfig.autoOrbit) {
            m_orbitalConfig.azimuth += m_orbitalConfig.autoOrbitSpeed * deltaTime;
            if (m_orbitalConfig.azimuth > 360.0f) {
                m_orbitalConfig.azimuth -= 360.0f;
            }
        }

        glm::vec3 orbitalPosition = calculateOrbitalPosition(centerPos);
        setPosition(orbitalPosition);
        setTarget(centerPos);
    }

    void CameraComponent::updateTransition(float deltaTime) {
        m_transitionState.transitionTime += deltaTime;
        
        float t = m_transitionState.transitionTime / m_transitionState.transitionDuration;
        if (t >= 1.0f) {
            // Transition complete
            t = 1.0f;
            m_transitionState.isTransitioning = false;
            m_currentType = m_transitionState.targetType;
            LOG_INFO("Camera transition completed");
        }

        // Apply smoothing
        float smoothT = smoothStep(t);

        // Interpolate position
        glm::vec3 newPosition = glm::mix(m_transitionState.startPosition, m_transitionState.targetPosition, smoothT);
        setPosition(newPosition);

        // Interpolate rotation
        glm::quat newRotation = glm::slerp(m_transitionState.startRotation, m_transitionState.targetRotation, smoothT);
        setRotationQuaternion(newRotation);
    }

    glm::vec3 CameraComponent::calculateThirdPersonPosition(const glm::vec3& targetPos) const {
        // Calculate position behind and above the target
        glm::vec3 offset = glm::vec3(0.0f, m_thirdPersonConfig.height, m_thirdPersonConfig.distance);
        
        // For now, we'll use a simple offset. In a real implementation, you'd
        // consider the target's rotation to position the camera properly
        return targetPos + offset + m_thirdPersonConfig.offset;
    }

    glm::vec3 CameraComponent::calculateOrbitalPosition(const glm::vec3& centerPos) const {
        float azimuthRad = glm::radians(m_orbitalConfig.azimuth);
        float elevationRad = glm::radians(m_orbitalConfig.elevation);
        
        float x = m_orbitalConfig.radius * cos(elevationRad) * cos(azimuthRad);
        float y = m_orbitalConfig.radius * sin(elevationRad);
        float z = m_orbitalConfig.radius * cos(elevationRad) * sin(azimuthRad);
        
        return centerPos + glm::vec3(x, y, z);
    }

    glm::vec3 CameraComponent::smoothInterpolate(const glm::vec3& start, const glm::vec3& end, float t) const {
        t = std::clamp(t, 0.0f, 1.0f);
        float smoothT = smoothStep(t);
        return glm::mix(start, end, smoothT);
    }

    void CameraComponent::markDirty() {
        m_viewDirty = true;
        m_rotationDirty = true;
    }

} // namespace IKore
