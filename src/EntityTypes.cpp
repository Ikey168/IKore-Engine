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

    void GameObject::serialize(SerializationData& data) const {
        data.makeObject();
        data["name"] = SerializationData(m_name);
        data["position"].setVec3(m_position);
        data["rotation"].setVec3(m_rotation);
        data["scale"].setVec3(m_scale);
        data["visible"] = SerializationData(m_visible);
        data["active"] = SerializationData(m_active);
    }

    bool GameObject::deserialize(const SerializationData& data) {
        if (!data.isObject()) {
            LOG_ERROR("GameObject deserialize: data is not an object");
            return false;
        }

        if (data.has("name")) {
            m_name = data.get("name").asString();
        }
        if (data.has("position")) {
            m_position = data.get("position").asVec3();
        }
        if (data.has("rotation")) {
            m_rotation = data.get("rotation").asVec3();
        }
        if (data.has("scale")) {
            m_scale = data.get("scale").asVec3();
        }
        if (data.has("visible")) {
            m_visible = data.get("visible").asBool();
        }
        if (data.has("active")) {
            m_active = data.get("active").asBool();
        }

        return true;
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

    void LightEntity::serialize(SerializationData& data) const {
        // Serialize base GameObject data first
        GameObject::serialize(data);

        // Add light-specific data
        data["lightType"] = SerializationData(static_cast<int64_t>(m_lightType));
        data["color"].setVec3(m_color);
        data["intensity"] = SerializationData(static_cast<double>(m_intensity));
        data["range"] = SerializationData(static_cast<double>(m_range));
        data["spotAngle"] = SerializationData(static_cast<double>(m_spotAngle));
    }

    bool LightEntity::deserialize(const SerializationData& data) {
        // Deserialize base GameObject data first
        if (!GameObject::deserialize(data)) {
            return false;
        }

        if (data.has("lightType")) {
            m_lightType = static_cast<LightType>(data.get("lightType").asInt());
        }
        if (data.has("color")) {
            m_color = data.get("color").asVec3();
        }
        if (data.has("intensity")) {
            m_intensity = static_cast<float>(data.get("intensity").asFloat());
        }
        if (data.has("range")) {
            m_range = static_cast<float>(data.get("range").asFloat());
        }
        if (data.has("spotAngle")) {
            m_spotAngle = static_cast<float>(data.get("spotAngle").asFloat());
        }

        return true;
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

    void CameraEntity::serialize(SerializationData& data) const {
        // Serialize base GameObject data first
        GameObject::serialize(data);

        // Add camera-specific data
        data["fieldOfView"] = SerializationData(static_cast<double>(m_fieldOfView));
        data["aspectRatio"] = SerializationData(static_cast<double>(m_aspectRatio));
        data["nearPlane"] = SerializationData(static_cast<double>(m_nearPlane));
        data["farPlane"] = SerializationData(static_cast<double>(m_farPlane));
    }

    bool CameraEntity::deserialize(const SerializationData& data) {
        // Deserialize base GameObject data first
        if (!GameObject::deserialize(data)) {
            return false;
        }

        if (data.has("fieldOfView")) {
            m_fieldOfView = static_cast<float>(data.get("fieldOfView").asFloat());
        }
        if (data.has("aspectRatio")) {
            m_aspectRatio = static_cast<float>(data.get("aspectRatio").asFloat());
        }
        if (data.has("nearPlane")) {
            m_nearPlane = static_cast<float>(data.get("nearPlane").asFloat());
        }
        if (data.has("farPlane")) {
            m_farPlane = static_cast<float>(data.get("farPlane").asFloat());
        }

        return true;
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

    void TestEntity::serialize(SerializationData& data) const {
        data.makeObject();
        data["message"] = SerializationData(m_message);
        data["lifetime"] = SerializationData(static_cast<double>(m_lifetime));
        data["age"] = SerializationData(static_cast<double>(m_age));
    }

    bool TestEntity::deserialize(const SerializationData& data) {
        if (!data.isObject()) {
            LOG_ERROR("TestEntity deserialize: data is not an object");
            return false;
        }

        if (data.has("message")) {
            m_message = data.get("message").asString();
        }
        if (data.has("lifetime")) {
            m_lifetime = static_cast<float>(data.get("lifetime").asFloat());
        }
        if (data.has("age")) {
            m_age = static_cast<float>(data.get("age").asFloat());
        }

        return true;
    }

} // namespace IKore
