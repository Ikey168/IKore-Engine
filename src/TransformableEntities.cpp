#include "TransformableEntities.h"
#include "Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace IKore {

    // ============================================================================
    // TransformableGameObject Implementation
    // ============================================================================

    TransformableGameObject::TransformableGameObject(const std::string& name, 
                                                   const glm::vec3& position,
                                                   const glm::vec3& rotation,
                                                   const glm::vec3& scale)
        : m_name(name)
        , m_transformComponent(position, rotation, scale)
        , m_visible(true)
        , m_parent(nullptr) {
        
        LOG_INFO("TransformableGameObject '" + m_name + "' created with ID: " + std::to_string(getID()));
    }

    void TransformableGameObject::initialize() {
        m_transformComponent.initialize();
        LOG_INFO("TransformableGameObject '" + m_name + "' initialized");
    }

    void TransformableGameObject::update(float deltaTime) {
        m_transformComponent.update(deltaTime);
        
        // Update children
        for (auto& child : m_children) {
            if (child && child->isValid()) {
                child->update(deltaTime);
            }
        }
    }

    void TransformableGameObject::cleanup() {
        // Remove from parent
        if (m_parent) {
            m_parent->removeChild(std::shared_ptr<TransformableGameObject>(this));
        }
        
        // Clean up children
        for (auto& child : m_children) {
            if (child) {
                child->cleanup();
            }
        }
        m_children.clear();
        
        m_transformComponent.cleanup();
        LOG_INFO("TransformableGameObject '" + m_name + "' cleaned up");
    }

    void TransformableGameObject::setParent(TransformableGameObject* parent) {
        if (m_parent == parent) {
            return; // No change
        }
        
        // Remove from current parent
        if (m_parent) {
            m_parent->removeChild(std::shared_ptr<TransformableGameObject>(this));
        }
        
        m_parent = parent;
        
        // Update transform hierarchy
        if (m_parent) {
            getTransform().setParent(&m_parent->getTransform());
        } else {
            getTransform().setParent(nullptr);
        }
    }

    void TransformableGameObject::addChild(std::shared_ptr<TransformableGameObject> child) {
        if (!child || child.get() == this) {
            return;
        }
        
        // Check if already a child
        auto it = std::find(m_children.begin(), m_children.end(), child);
        if (it != m_children.end()) {
            return; // Already a child
        }
        
        m_children.push_back(child);
        child->setParent(this);
    }

    void TransformableGameObject::removeChild(std::shared_ptr<TransformableGameObject> child) {
        if (!child) {
            return;
        }
        
        auto it = std::find(m_children.begin(), m_children.end(), child);
        if (it != m_children.end()) {
            (*it)->setParent(nullptr);
            m_children.erase(it);
        }
    }

    glm::mat4 TransformableGameObject::getWorldMatrix() const {
        return getTransform().getWorldMatrix();
    }

    glm::vec3 TransformableGameObject::getPosition() const {
        return getTransform().getWorldPosition();
    }

    // ============================================================================
    // TransformableLight Implementation
    // ============================================================================

    TransformableLight::TransformableLight(const std::string& name,
                                         LightType type,
                                         const glm::vec3& position,
                                         const glm::vec3& color,
                                         float intensity)
        : m_name(name)
        , m_transformComponent(position)
        , m_type(type)
        , m_color(color)
        , m_intensity(intensity)
        , m_enabled(true) {
        
        LOG_INFO("TransformableLight '" + m_name + "' (" + getTypeString() + ") created with ID: " + std::to_string(getID()));
    }

    void TransformableLight::initialize() {
        m_transformComponent.initialize();
        LOG_INFO("TransformableLight '" + m_name + "' initialized");
    }

    void TransformableLight::update(float deltaTime) {
        m_transformComponent.update(deltaTime);
        
        // Add any light-specific update logic here
        // For example, animated lights, flickering, etc.
    }

    void TransformableLight::cleanup() {
        m_transformComponent.cleanup();
        LOG_INFO("TransformableLight '" + m_name + "' cleaned up");
    }

    glm::vec3 TransformableLight::getDirection() const {
        if (m_type == LightType::DIRECTIONAL || m_type == LightType::SPOT) {
            return getTransform().getForward();
        }
        return glm::vec3(0.0f, -1.0f, 0.0f); // Default down direction
    }

    void TransformableLight::setDirection(const glm::vec3& direction) {
        if (m_type == LightType::DIRECTIONAL || m_type == LightType::SPOT) {
            // Calculate rotation to face the given direction
            glm::vec3 currentPos = getTransform().getWorldPosition();
            glm::vec3 targetPos = currentPos + direction;
            getTransform().lookAt(targetPos);
        }
    }

    glm::vec3 TransformableLight::getWorldPosition() const {
        return getTransform().getWorldPosition();
    }

    glm::vec3 TransformableLight::getPosition() const {
        return getWorldPosition();
    }

    std::string TransformableLight::getTypeString() const {
        switch (m_type) {
            case LightType::DIRECTIONAL: return "Directional";
            case LightType::POINT: return "Point";
            case LightType::SPOT: return "Spot";
            case LightType::AREA: return "Area";
            default: return "Unknown";
        }
    }

    // ============================================================================
    // TransformableCamera Implementation
    // ============================================================================

    TransformableCamera::TransformableCamera(const std::string& name,
                                           const glm::vec3& position,
                                           const glm::vec3& rotation,
                                           float fov,
                                           float aspectRatio,
                                           float nearPlane,
                                           float farPlane)
        : m_name(name)
        , m_transformComponent(position, rotation)
        , m_projectionType(ProjectionType::PERSPECTIVE)
        , m_fov(fov)
        , m_aspectRatio(aspectRatio)
        , m_nearPlane(nearPlane)
        , m_farPlane(farPlane)
        , m_active(true) {
        
        LOG_INFO("TransformableCamera '" + m_name + "' created with ID: " + std::to_string(getID()));
    }

    void TransformableCamera::initialize() {
        m_transformComponent.initialize();
        LOG_INFO("TransformableCamera '" + m_name + "' initialized");
    }

    void TransformableCamera::update(float deltaTime) {
        m_transformComponent.update(deltaTime);
        
        // Add any camera-specific update logic here
        // For example, smooth following, orbital movement, etc.
    }

    void TransformableCamera::cleanup() {
        m_transformComponent.cleanup();
        LOG_INFO("TransformableCamera '" + m_name + "' cleaned up");
    }

    glm::mat4 TransformableCamera::getViewMatrix() const {
        // For cameras, we need the inverse of the world matrix
        return glm::inverse(getTransform().getWorldMatrix());
    }

    glm::mat4 TransformableCamera::getProjectionMatrix() const {
        if (m_projectionType == ProjectionType::PERSPECTIVE) {
            return glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearPlane, m_farPlane);
        } else {
            // Orthographic projection
            float halfHeight = m_fov; // Use FOV as orthographic height
            float halfWidth = halfHeight * m_aspectRatio;
            return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, m_nearPlane, m_farPlane);
        }
    }

    glm::mat4 TransformableCamera::getViewProjectionMatrix() const {
        return getProjectionMatrix() * getViewMatrix();
    }

    void TransformableCamera::lookAt(const glm::vec3& target, const glm::vec3& up) {
        getTransform().lookAt(target, up);
    }

    glm::vec3 TransformableCamera::getWorldPosition() const {
        return getTransform().getWorldPosition();
    }

    glm::vec3 TransformableCamera::getPosition() const {
        return getWorldPosition();
    }

    glm::vec3 TransformableCamera::getForward() const {
        return getTransform().getForward();
    }

    glm::vec3 TransformableCamera::getRight() const {
        return getTransform().getRight();
    }

    glm::vec3 TransformableCamera::getUp() const {
        return getTransform().getUp();
    }

} // namespace IKore
