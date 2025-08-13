#pragma once

#include "Entity.h"
#include "Transform.h"
#include <glm/glm.hpp>
#include <string>
#include <memory>

namespace IKore {

    /**
     * @brief Enhanced GameObject with Transform Component
     * 
     * This class extends the basic GameObject to include a Transform component,
     * allowing for hierarchical transformations and proper 3D positioning.
     */
    class TransformableGameObject : public Entity {
    public:
        /**
         * @brief Constructor with name and optional transform
         * @param name Object name
         * @param position Initial position (default: origin)
         * @param rotation Initial rotation in degrees (default: no rotation)
         * @param scale Initial scale (default: unit scale)
         */
        TransformableGameObject(const std::string& name, 
                               const glm::vec3& position = glm::vec3(0.0f),
                               const glm::vec3& rotation = glm::vec3(0.0f),
                               const glm::vec3& scale = glm::vec3(1.0f));

        /**
         * @brief Virtual destructor
         */
        virtual ~TransformableGameObject() = default;

        /**
         * @brief Initialize the transformable game object
         */
        virtual void initialize() override;

        /**
         * @brief Update the transformable game object
         * @param deltaTime Time since last update
         */
        virtual void update(float deltaTime) override;

        /**
         * @brief Cleanup the transformable game object
         */
        virtual void cleanup() override;

        /**
         * @brief Get the object name
         * @return Object name
         */
        const std::string& getName() const { return m_name; }

        /**
         * @brief Set the object name
         * @param name New name
         */
        void setName(const std::string& name) { m_name = name; }

        /**
         * @brief Get the transform component
         * @return Reference to transform component
         */
        TransformComponent& getTransformComponent() { return m_transformComponent; }

        /**
         * @brief Get the transform component (const version)
         * @return Const reference to transform component
         */
        const TransformComponent& getTransformComponent() const { return m_transformComponent; }

        /**
         * @brief Get the transform (convenience method)
         * @return Reference to transform
         */
        Transform& getTransform() { return m_transformComponent.getTransform(); }

        /**
         * @brief Get the transform (const convenience method)
         * @return Const reference to transform
         */
        const Transform& getTransform() const { return m_transformComponent.getTransform(); }

        /**
         * @brief Set parent transform (for hierarchical transformations)
         * @param parent Pointer to parent transformable object (nullptr for no parent)
         */
        void setParent(TransformableGameObject* parent);

        /**
         * @brief Get parent transformable object
         * @return Pointer to parent (nullptr if no parent)
         */
        TransformableGameObject* getParent() const { return m_parent; }

        /**
         * @brief Add a child transformable object
         * @param child Pointer to child object
         */
        void addChild(std::shared_ptr<TransformableGameObject> child);

        /**
         * @brief Remove a child transformable object
         * @param child Pointer to child object
         */
        void removeChild(std::shared_ptr<TransformableGameObject> child);

        /**
         * @brief Get all children
         * @return Vector of child objects
         */
        const std::vector<std::shared_ptr<TransformableGameObject>>& getChildren() const { return m_children; }

        /**
         * @brief Get the world transformation matrix for rendering
         * @return World transformation matrix
         */
        glm::mat4 getWorldMatrix() const;

        /**
         * @brief Get entity position (override from Entity)
         * @return Entity position
         */
        glm::vec3 getPosition() const override;

        /**
         * @brief Check if this object is visible (can be used for culling)
         * @return True if visible
         */
        virtual bool isVisible() const { return m_visible; }

        /**
         * @brief Set visibility
         * @param visible Visibility state
         */
        void setVisible(bool visible) { m_visible = visible; }

    protected:
        std::string m_name;                                                ///< Object name
        TransformComponent m_transformComponent;                          ///< Transform component
        bool m_visible;                                                   ///< Visibility flag
        
        // Hierarchy management (separate from Transform hierarchy for type safety)
        TransformableGameObject* m_parent;                                ///< Parent object
        std::vector<std::shared_ptr<TransformableGameObject>> m_children; ///< Child objects
    };

    /**
     * @brief Enhanced Light Entity with Transform Component
     * 
     * Light entity that can be positioned and oriented in 3D space using transforms.
     */
    class TransformableLight : public Entity {
    public:
        /**
         * @brief Light types
         */
        enum class LightType {
            DIRECTIONAL,    ///< Directional light (like sun)
            POINT,          ///< Point light (like bulb)
            SPOT,           ///< Spot light (like flashlight)
            AREA            ///< Area light (like panel)
        };

        /**
         * @brief Constructor
         * @param name Light name
         * @param type Light type
         * @param position Initial position
         * @param color Light color (RGB)
         * @param intensity Light intensity
         */
        TransformableLight(const std::string& name,
                          LightType type = LightType::POINT,
                          const glm::vec3& position = glm::vec3(0.0f),
                          const glm::vec3& color = glm::vec3(1.0f),
                          float intensity = 1.0f);

        /**
         * @brief Virtual destructor
         */
        virtual ~TransformableLight() = default;

        /**
         * @brief Initialize the light
         */
        virtual void initialize() override;

        /**
         * @brief Update the light
         * @param deltaTime Time since last update
         */
        virtual void update(float deltaTime) override;

        /**
         * @brief Cleanup the light
         */
        virtual void cleanup() override;

        /**
         * @brief Get the light name
         * @return Light name
         */
        const std::string& getName() const { return m_name; }

        /**
         * @brief Set the light name
         * @param name New name
         */
        void setName(const std::string& name) { m_name = name; }

        /**
         * @brief Get the transform component
         * @return Reference to transform component
         */
        TransformComponent& getTransformComponent() { return m_transformComponent; }

        /**
         * @brief Get the transform component (const version)
         * @return Const reference to transform component
         */
        const TransformComponent& getTransformComponent() const { return m_transformComponent; }

        /**
         * @brief Get the transform (convenience method)
         * @return Reference to transform
         */
        Transform& getTransform() { return m_transformComponent.getTransform(); }

        /**
         * @brief Get the transform (const convenience method)
         * @return Const reference to transform
         */
        const Transform& getTransform() const { return m_transformComponent.getTransform(); }

        /**
         * @brief Get light type
         * @return Light type
         */
        LightType getType() const { return m_type; }

        /**
         * @brief Set light type
         * @param type New light type
         */
        void setType(LightType type) { m_type = type; }

        /**
         * @brief Get light color
         * @return Light color (RGB)
         */
        const glm::vec3& getColor() const { return m_color; }

        /**
         * @brief Set light color
         * @param color New color (RGB)
         */
        void setColor(const glm::vec3& color) { m_color = color; }

        /**
         * @brief Get light intensity
         * @return Light intensity
         */
        float getIntensity() const { return m_intensity; }

        /**
         * @brief Set light intensity
         * @param intensity New intensity
         */
        void setIntensity(float intensity) { m_intensity = intensity; }

        /**
         * @brief Get light direction (for directional and spot lights)
         * @return Direction vector (world space)
         */
        glm::vec3 getDirection() const;

        /**
         * @brief Set light direction (for directional and spot lights)
         * @param direction Direction vector
         */
        void setDirection(const glm::vec3& direction);

        /**
         * @brief Get world position
         * @return World position
         */
        glm::vec3 getWorldPosition() const;

        /**
         * @brief Get entity position (override from Entity)
         * @return Entity position
         */
        glm::vec3 getPosition() const override;

        /**
         * @brief Check if light is enabled
         * @return True if enabled
         */
        bool isEnabled() const { return m_enabled; }

        /**
         * @brief Set light enabled state
         * @param enabled Enabled state
         */
        void setEnabled(bool enabled) { m_enabled = enabled; }

        /**
         * @brief Get light type as string
         * @return String representation of light type
         */
        std::string getTypeString() const;

    private:
        std::string m_name;                  ///< Light name
        TransformComponent m_transformComponent; ///< Transform component
        LightType m_type;                    ///< Light type
        glm::vec3 m_color;                   ///< Light color (RGB)
        float m_intensity;                   ///< Light intensity
        bool m_enabled;                      ///< Enabled state
    };

    /**
     * @brief Enhanced Camera Entity with Transform Component
     * 
     * Camera entity that can be positioned and oriented in 3D space using transforms.
     */
    class TransformableCamera : public Entity {
    public:
        /**
         * @brief Camera projection types
         */
        enum class ProjectionType {
            PERSPECTIVE,    ///< Perspective projection
            ORTHOGRAPHIC    ///< Orthographic projection
        };

        /**
         * @brief Constructor
         * @param name Camera name
         * @param position Initial position
         * @param rotation Initial rotation
         * @param fov Field of view in degrees (for perspective)
         * @param aspectRatio Aspect ratio (width/height)
         * @param nearPlane Near clipping plane
         * @param farPlane Far clipping plane
         */
        TransformableCamera(const std::string& name,
                           const glm::vec3& position = glm::vec3(0.0f, 0.0f, 5.0f),
                           const glm::vec3& rotation = glm::vec3(0.0f),
                           float fov = 45.0f,
                           float aspectRatio = 16.0f / 9.0f,
                           float nearPlane = 0.1f,
                           float farPlane = 100.0f);

        /**
         * @brief Virtual destructor
         */
        virtual ~TransformableCamera() = default;

        /**
         * @brief Initialize the camera
         */
        virtual void initialize() override;

        /**
         * @brief Update the camera
         * @param deltaTime Time since last update
         */
        virtual void update(float deltaTime) override;

        /**
         * @brief Cleanup the camera
         */
        virtual void cleanup() override;

        /**
         * @brief Get the camera name
         * @return Camera name
         */
        const std::string& getName() const { return m_name; }

        /**
         * @brief Set the camera name
         * @param name New name
         */
        void setName(const std::string& name) { m_name = name; }

        /**
         * @brief Get the transform component
         * @return Reference to transform component
         */
        TransformComponent& getTransformComponent() { return m_transformComponent; }

        /**
         * @brief Get the transform component (const version)
         * @return Const reference to transform component
         */
        const TransformComponent& getTransformComponent() const { return m_transformComponent; }

        /**
         * @brief Get the transform (convenience method)
         * @return Reference to transform
         */
        Transform& getTransform() { return m_transformComponent.getTransform(); }

        /**
         * @brief Get the transform (const convenience method)
         * @return Const reference to transform
         */
        const Transform& getTransform() const { return m_transformComponent.getTransform(); }

        /**
         * @brief Get view matrix
         * @return View matrix for rendering
         */
        glm::mat4 getViewMatrix() const;

        /**
         * @brief Get projection matrix
         * @return Projection matrix for rendering
         */
        glm::mat4 getProjectionMatrix() const;

        /**
         * @brief Get combined view-projection matrix
         * @return View-projection matrix
         */
        glm::mat4 getViewProjectionMatrix() const;

        /**
         * @brief Look at a target position
         * @param target Target position to look at
         * @param up Up vector (default: world Y-axis)
         */
        void lookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));

        /**
         * @brief Get field of view
         * @return Field of view in degrees
         */
        float getFOV() const { return m_fov; }

        /**
         * @brief Set field of view
         * @param fov Field of view in degrees
         */
        void setFOV(float fov) { m_fov = fov; }

        /**
         * @brief Get aspect ratio
         * @return Aspect ratio (width/height)
         */
        float getAspectRatio() const { return m_aspectRatio; }

        /**
         * @brief Set aspect ratio
         * @param aspectRatio New aspect ratio
         */
        void setAspectRatio(float aspectRatio) { m_aspectRatio = aspectRatio; }

        /**
         * @brief Get near clipping plane
         * @return Near plane distance
         */
        float getNearPlane() const { return m_nearPlane; }

        /**
         * @brief Set near clipping plane
         * @param nearPlane Near plane distance
         */
        void setNearPlane(float nearPlane) { m_nearPlane = nearPlane; }

        /**
         * @brief Get far clipping plane
         * @return Far plane distance
         */
        float getFarPlane() const { return m_farPlane; }

        /**
         * @brief Set far clipping plane
         * @param farPlane Far plane distance
         */
        void setFarPlane(float farPlane) { m_farPlane = farPlane; }

        /**
         * @brief Get projection type
         * @return Projection type
         */
        ProjectionType getProjectionType() const { return m_projectionType; }

        /**
         * @brief Set projection type
         * @param type Projection type
         */
        void setProjectionType(ProjectionType type) { m_projectionType = type; }

        /**
         * @brief Check if camera is active
         * @return True if active
         */
        bool isActive() const { return m_active; }

        /**
         * @brief Set camera active state
         * @param active Active state
         */
        void setActive(bool active) { m_active = active; }

        /**
         * @brief Get world position
         * @return World position
         */
        glm::vec3 getWorldPosition() const;

        /**
         * @brief Get entity position (override from Entity)
         * @return Entity position
         */
        glm::vec3 getPosition() const override;

        /**
         * @brief Get forward direction
         * @return Forward direction vector
         */
        glm::vec3 getForward() const;

        /**
         * @brief Get right direction
         * @return Right direction vector
         */
        glm::vec3 getRight() const;

        /**
         * @brief Get up direction
         * @return Up direction vector
         */
        glm::vec3 getUp() const;

    private:
        std::string m_name;                      ///< Camera name
        TransformComponent m_transformComponent; ///< Transform component
        ProjectionType m_projectionType;         ///< Projection type
        float m_fov;                            ///< Field of view (degrees)
        float m_aspectRatio;                    ///< Aspect ratio
        float m_nearPlane;                      ///< Near clipping plane
        float m_farPlane;                       ///< Far clipping plane
        bool m_active;                          ///< Active state
    };

} // namespace IKore
