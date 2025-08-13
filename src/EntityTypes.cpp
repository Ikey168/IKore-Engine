#include "EntityTypes.h"
#include "Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace IKore {

    // ============================================================================
    // GameObject Implementation
    // ============================================================================

    GameObject::GameObject(const std::string& name, const glm::vec3& position)
        : Entity(), m_name(name), m_position(position), m_rotation(0.0f), m_scale(1.0f), 
          m_visible(true), m_active(true) {
        LOG_INFO("GameObject '" + m_name + "' created with ID: " + std::to_string(getID()));
    }

    void GameObject::update(float deltaTime) {
        Entity::update(deltaTime);
        
        if (!m_active) {
            return;
        }

        // Base GameObject update logic can be extended by derived classes
    }

    void GameObject::initialize() {
        Entity::initialize();
        LOG_INFO("GameObject '" + m_name + "' initialized (ID: " + std::to_string(getID()) + ")");
    }

    void GameObject::cleanup() {
        Entity::cleanup();
        LOG_INFO("GameObject '" + m_name + "' cleaned up (ID: " + std::to_string(getID()) + ")");
    }

    void GameObject::translate(const glm::vec3& offset) {
        m_position += offset;
    }

    void GameObject::rotate(const glm::vec3& eulerAngles) {
        m_rotation += eulerAngles;
    }

    void GameObject::scale(const glm::vec3& factor) {
        m_scale *= factor;
    }

    glm::mat4 GameObject::getTransformMatrix() const {
        glm::mat4 transform = glm::mat4(1.0f);
        
        // Apply transformations in order: Scale -> Rotate -> Translate
        transform = glm::translate(transform, m_position);
        transform = glm::rotate(transform, glm::radians(m_rotation.z), glm::vec3(0, 0, 1));
        transform = glm::rotate(transform, glm::radians(m_rotation.y), glm::vec3(0, 1, 0));
        transform = glm::rotate(transform, glm::radians(m_rotation.x), glm::vec3(1, 0, 0));
        transform = glm::scale(transform, m_scale);
        
        return transform;
    }

    // ============================================================================
    // LightEntity Implementation
    // ============================================================================

    LightEntity::LightEntity(const std::string& name, LightType type)
        : GameObject(name), m_lightType(type), m_color(1.0f, 1.0f, 1.0f), 
          m_intensity(1.0f), m_range(10.0f), m_spotAngle(45.0f) {
        LOG_INFO("LightEntity '" + getName() + "' created with ID: " + std::to_string(getID()));
    }

    void LightEntity::update(float deltaTime) {
        GameObject::update(deltaTime);
        
        if (!isActive()) {
            return;
        }

        // Light-specific update logic
        // Could include light animation, flickering, etc.
    }

    void LightEntity::initialize() {
        GameObject::initialize();
        
        std::string typeStr;
        switch (m_lightType) {
            case LightType::DIRECTIONAL: typeStr = "Directional"; break;
            case LightType::POINT: typeStr = "Point"; break;
            case LightType::SPOT: typeStr = "Spot"; break;
        }
        
        LOG_INFO("LightEntity '" + getName() + "' (" + typeStr + ") initialized");
    }

    // ============================================================================
    // CameraEntity Implementation
    // ============================================================================

    CameraEntity::CameraEntity(const std::string& name)
        : GameObject(name), m_fieldOfView(45.0f), m_aspectRatio(16.0f / 9.0f), 
          m_nearPlane(0.1f), m_farPlane(100.0f) {
        LOG_INFO("CameraEntity '" + getName() + "' created with ID: " + std::to_string(getID()));
    }

    glm::mat4 CameraEntity::getViewMatrix() const {
        glm::vec3 front = getForward();
        glm::vec3 up = getUp();
        return glm::lookAt(getPosition(), getPosition() + front, up);
    }

    glm::mat4 CameraEntity::getProjectionMatrix() const {
        return glm::perspective(glm::radians(m_fieldOfView), m_aspectRatio, m_nearPlane, m_farPlane);
    }

    glm::vec3 CameraEntity::getForward() const {
        glm::vec3 front;
        glm::vec3 rot = getRotation();
        
        front.x = cos(glm::radians(rot.y)) * cos(glm::radians(rot.x));
        front.y = sin(glm::radians(rot.x));
        front.z = sin(glm::radians(rot.y)) * cos(glm::radians(rot.x));
        
        return glm::normalize(front);
    }

    glm::vec3 CameraEntity::getRight() const {
        return glm::normalize(glm::cross(getForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    glm::vec3 CameraEntity::getUp() const {
        return glm::normalize(glm::cross(getRight(), getForward()));
    }

    void CameraEntity::update(float deltaTime) {
        GameObject::update(deltaTime);
        
        if (!isActive()) {
            return;
        }

        // Camera-specific update logic
        // Could include camera movement, target following, etc.
    }

    void CameraEntity::initialize() {
        GameObject::initialize();
        LOG_INFO("CameraEntity '" + getName() + "' initialized (FOV: " + 
                 std::to_string(m_fieldOfView) + ", Aspect: " + std::to_string(m_aspectRatio) + ")");
    }

    // ============================================================================
    // TestEntity Implementation
    // ============================================================================

    TestEntity::TestEntity(const std::string& message)
        : Entity(), m_message(message), m_lifetime(0.0f), m_age(0.0f) {
        LOG_INFO("TestEntity created with message: '" + m_message + "' (ID: " + std::to_string(getID()) + ")");
    }

    void TestEntity::update(float deltaTime) {
        Entity::update(deltaTime);
        
        m_age += deltaTime;
        
        // Check if entity has expired
        if (isExpired()) {
            LOG_INFO("TestEntity '" + m_message + "' has expired after " + std::to_string(m_age) + " seconds");
            invalidate();
        }
    }

    void TestEntity::initialize() {
        Entity::initialize();
        LOG_INFO("TestEntity initialized: '" + m_message + "' (Lifetime: " + 
                 (m_lifetime > 0.0f ? std::to_string(m_lifetime) + "s" : "infinite") + ")");
    }

    void TestEntity::cleanup() {
        Entity::cleanup();
        LOG_INFO("TestEntity '" + m_message + "' cleaned up after " + std::to_string(m_age) + " seconds");
    }

} // namespace IKore
