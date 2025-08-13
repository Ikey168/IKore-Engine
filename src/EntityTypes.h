#pragma once

#include "Entity.h"
#include <glm/glm.hpp>
#include <string>

namespace IKore {

    /**
     * @brief Example GameObject entity with transform and basic properties
     */
    class GameObject : public Entity {
    public:
        GameObject(const std::string& name = "GameObject", const glm::vec3& position = glm::vec3(0.0f));
        virtual ~GameObject() = default;

        // Transform properties
        void setPosition(const glm::vec3& position) { m_position = position; }
        void setRotation(const glm::vec3& rotation) { m_rotation = rotation; }
        void setScale(const glm::vec3& scale) { m_scale = scale; }

        const glm::vec3& getPosition() const { return m_position; }
        const glm::vec3& getRotation() const { return m_rotation; }
        const glm::vec3& getScale() const { return m_scale; }

        // Name and visibility
        void setName(const std::string& name) { m_name = name; }
        const std::string& getName() const { return m_name; }
        
        void setVisible(bool visible) { m_visible = visible; }
        bool isVisible() const { return m_visible; }

        void setActive(bool active) { m_active = active; }
        bool isActive() const { return m_active; }

        // Override base Entity methods
        void update(float deltaTime) override;
        void initialize() override;
        void cleanup() override;

        // Transform methods
        void translate(const glm::vec3& offset);
        void rotate(const glm::vec3& eulerAngles);
        void scale(const glm::vec3& factor);

        // Get world transform matrix
        glm::mat4 getTransformMatrix() const;

    protected:
        std::string m_name;
        glm::vec3 m_position;
        glm::vec3 m_rotation; // Euler angles in degrees
        glm::vec3 m_scale;
        bool m_visible;
        bool m_active;
    };

    /**
     * @brief Example Light entity
     */
    class LightEntity : public GameObject {
    public:
        enum class LightType {
            DIRECTIONAL,
            POINT,
            SPOT
        };

        LightEntity(const std::string& name = "Light", LightType type = LightType::POINT);
        virtual ~LightEntity() = default;

        // Light properties
        void setLightType(LightType type) { m_lightType = type; }
        LightType getLightType() const { return m_lightType; }

        void setColor(const glm::vec3& color) { m_color = color; }
        const glm::vec3& getColor() const { return m_color; }

        void setIntensity(float intensity) { m_intensity = intensity; }
        float getIntensity() const { return m_intensity; }

        void setRange(float range) { m_range = range; }
        float getRange() const { return m_range; }

        // For spot lights
        void setSpotAngle(float angle) { m_spotAngle = angle; }
        float getSpotAngle() const { return m_spotAngle; }

        // Override update for light-specific behavior
        void update(float deltaTime) override;
        void initialize() override;

    private:
        LightType m_lightType;
        glm::vec3 m_color;
        float m_intensity;
        float m_range;
        float m_spotAngle; // For spot lights
    };

    /**
     * @brief Example Camera entity
     */
    class CameraEntity : public GameObject {
    public:
        CameraEntity(const std::string& name = "Camera");
        virtual ~CameraEntity() = default;

        // Camera properties
        void setFieldOfView(float fov) { m_fieldOfView = fov; }
        float getFieldOfView() const { return m_fieldOfView; }

        void setAspectRatio(float aspect) { m_aspectRatio = aspect; }
        float getAspectRatio() const { return m_aspectRatio; }

        void setNearPlane(float nearPlane) { m_nearPlane = nearPlane; }
        float getNearPlane() const { return m_nearPlane; }

        void setFarPlane(float farPlane) { m_farPlane = farPlane; }
        float getFarPlane() const { return m_farPlane; }

        // Camera matrices
        glm::mat4 getViewMatrix() const;
        glm::mat4 getProjectionMatrix() const;

        // Camera direction vectors
        glm::vec3 getForward() const;
        glm::vec3 getRight() const;
        glm::vec3 getUp() const;

        // Override update for camera-specific behavior
        void update(float deltaTime) override;
        void initialize() override;

    private:
        float m_fieldOfView;
        float m_aspectRatio;
        float m_nearPlane;
        float m_farPlane;
    };

    /**
     * @brief Simple test entity for demonstration
     */
    class TestEntity : public Entity {
    public:
        TestEntity(const std::string& message = "Test Entity");
        virtual ~TestEntity() = default;

        void setMessage(const std::string& message) { m_message = message; }
        const std::string& getMessage() const { return m_message; }

        void setLifetime(float lifetime) { m_lifetime = lifetime; }
        float getLifetime() const { return m_lifetime; }
        float getAge() const { return m_age; }

        bool isExpired() const { return m_lifetime > 0.0f && m_age >= m_lifetime; }

        // Override Entity methods
        void update(float deltaTime) override;
        void initialize() override;
        void cleanup() override;

    private:
        std::string m_message;
        float m_lifetime; // 0 means infinite lifetime
        float m_age;
    };

} // namespace IKore
