#pragma once

#include "Entity.h"
#include "InputComponent.h"
#include "Transform.h"
#include "TransformableEntities.h"
#include <glm/glm.hpp>
#include <string>
#include <memory>

namespace IKore {

    /**
     * @brief Controllable Game Object with Input Component
     * 
     * This class extends TransformableGameObject to include input controls,
     * allowing for keyboard and mouse-based movement and interaction.
     */
    class ControllableGameObject : public TransformableGameObject {
    public:
        /**
         * @brief Movement mode for the controllable object
         */
        enum class MovementMode {
            WALKING,        ///< Ground-based walking (XZ plane)
            FLYING,         ///< Free flight in 3D space
            CAMERA_STYLE    ///< Camera-style controls (WASD + mouse look)
        };

        /**
         * @brief Constructor
         * @param name Object name
         * @param position Initial position
         * @param movementMode Movement control mode
         */
        ControllableGameObject(const std::string& name, 
                              const glm::vec3& position = glm::vec3(0.0f),
                              MovementMode movementMode = MovementMode::WALKING);

        /**
         * @brief Virtual destructor
         */
        virtual ~ControllableGameObject() = default;

        /**
         * @brief Initialize the controllable game object
         */
        virtual void initialize() override;

        /**
         * @brief Update the controllable game object
         * @param deltaTime Time since last update
         */
        virtual void update(float deltaTime) override;

        /**
         * @brief Cleanup the controllable game object
         */
        virtual void cleanup() override;

        /**
         * @brief Get the input component
         * @return Reference to input component
         */
        InputComponent& getInputComponent() { return m_inputComponent; }

        /**
         * @brief Get the input component (const version)
         * @return Const reference to input component
         */
        const InputComponent& getInputComponent() const { return m_inputComponent; }

        /**
         * @brief Set movement speed
         * @param speed Movement speed in units per second
         */
        void setMovementSpeed(float speed) { m_movementSpeed = speed; }

        /**
         * @brief Get movement speed
         * @return Current movement speed
         */
        float getMovementSpeed() const { return m_movementSpeed; }

        /**
         * @brief Set rotation speed for mouse look
         * @param speed Rotation speed in degrees per pixel
         */
        void setRotationSpeed(float speed) { m_rotationSpeed = speed; }

        /**
         * @brief Get rotation speed
         * @return Current rotation speed
         */
        float getRotationSpeed() const { return m_rotationSpeed; }

        /**
         * @brief Set movement mode
         * @param mode New movement mode
         */
        void setMovementMode(MovementMode mode);

        /**
         * @brief Get movement mode
         * @return Current movement mode
         */
        MovementMode getMovementMode() const { return m_movementMode; }

        /**
         * @brief Enable/disable input processing
         * @param enabled True to enable input
         */
        void setInputEnabled(bool enabled) { m_inputComponent.setEnabled(enabled); }

        /**
         * @brief Check if input is enabled
         * @return True if input is enabled
         */
        bool isInputEnabled() const { return m_inputComponent.isEnabled(); }

        /**
         * @brief Set whether this object captures mouse for camera-style look
         * @param capture True to capture mouse
         */
        void setMouseCapture(bool capture);

        /**
         * @brief Check if mouse is captured
         * @return True if mouse is captured
         */
        bool isMouseCaptured() const { return m_mouseCaptured; }

    protected:
        /**
         * @brief Setup default input bindings based on movement mode
         */
        virtual void setupInputBindings();

        /**
         * @brief Handle movement input
         * @param event Input event
         */
        virtual void handleMovementInput(const InputEvent& event);

        /**
         * @brief Handle mouse look input
         * @param event Input event
         */
        virtual void handleMouseLook(const InputEvent& event);

        /**
         * @brief Handle action input (interaction, jumping, etc.)
         * @param event Input event
         */
        virtual void handleActionInput(const InputEvent& event);

        /**
         * @brief Handle mouse scroll input (zoom, speed adjustment, etc.)
         * @param event Input event
         */
        virtual void handleMouseScroll(const InputEvent& event);

    private:
        InputComponent m_inputComponent;    ///< Input component for handling controls
        MovementMode m_movementMode;        ///< Current movement mode
        float m_movementSpeed;              ///< Movement speed (units per second)
        float m_rotationSpeed;              ///< Rotation speed (degrees per pixel)
        bool m_mouseCaptured;               ///< Mouse capture state
        
        // Movement state
        glm::vec3 m_movementDirection;      ///< Current movement direction
        glm::vec2 m_rotation;               ///< Current rotation (pitch, yaw)
        bool m_movingForward;               ///< Forward movement state
        bool m_movingBackward;              ///< Backward movement state
        bool m_movingLeft;                  ///< Left movement state
        bool m_movingRight;                 ///< Right movement state
        bool m_movingUp;                    ///< Up movement state (flying mode)
        bool m_movingDown;                  ///< Down movement state (flying mode)
    };

    /**
     * @brief Controllable Camera with Input Component
     * 
     * This class extends TransformableCamera to include first-person or third-person
     * camera controls with keyboard and mouse input.
     */
    class ControllableCamera : public TransformableCamera {
    public:
        /**
         * @brief Camera control mode
         */
        enum class CameraMode {
            FIRST_PERSON,   ///< First-person camera (mouse look + WASD)
            THIRD_PERSON,   ///< Third-person camera (orbit around target)
            FREE_FLY        ///< Free-flying camera
        };

        /**
         * @brief Constructor
         * @param name Camera name
         * @param position Initial position
         * @param mode Camera control mode
         */
        ControllableCamera(const std::string& name,
                          const glm::vec3& position = glm::vec3(0.0f, 0.0f, 5.0f),
                          CameraMode mode = CameraMode::FIRST_PERSON);

        /**
         * @brief Virtual destructor
         */
        virtual ~ControllableCamera() = default;

        /**
         * @brief Initialize the controllable camera
         */
        virtual void initialize() override;

        /**
         * @brief Update the controllable camera
         * @param deltaTime Time since last update
         */
        virtual void update(float deltaTime) override;

        /**
         * @brief Cleanup the controllable camera
         */
        virtual void cleanup() override;

        /**
         * @brief Get the input component
         * @return Reference to input component
         */
        InputComponent& getInputComponent() { return m_inputComponent; }

        /**
         * @brief Get the input component (const version)
         * @return Const reference to input component
         */
        const InputComponent& getInputComponent() const { return m_inputComponent; }

        /**
         * @brief Set camera mode
         * @param mode New camera mode
         */
        void setCameraMode(CameraMode mode);

        /**
         * @brief Get camera mode
         * @return Current camera mode
         */
        CameraMode getCameraMode() const { return m_cameraMode; }

        /**
         * @brief Set movement speed
         * @param speed Movement speed in units per second
         */
        void setMovementSpeed(float speed) { m_movementSpeed = speed; }

        /**
         * @brief Get movement speed
         * @return Current movement speed
         */
        float getMovementSpeed() const { return m_movementSpeed; }

        /**
         * @brief Set mouse sensitivity
         * @param sensitivity Mouse sensitivity multiplier
         */
        void setMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }

        /**
         * @brief Get mouse sensitivity
         * @return Current mouse sensitivity
         */
        float getMouseSensitivity() const { return m_mouseSensitivity; }

        /**
         * @brief Set target for third-person mode
         * @param target Target position to orbit around
         */
        void setTarget(const glm::vec3& target) { m_target = target; }

        /**
         * @brief Get target position
         * @return Current target position
         */
        const glm::vec3& getTarget() const { return m_target; }

        /**
         * @brief Set orbit distance for third-person mode
         * @param distance Distance from target
         */
        void setOrbitDistance(float distance) { m_orbitDistance = distance; }

        /**
         * @brief Get orbit distance
         * @return Current orbit distance
         */
        float getOrbitDistance() const { return m_orbitDistance; }

        /**
         * @brief Enable/disable input processing
         * @param enabled True to enable input
         */
        void setInputEnabled(bool enabled) { m_inputComponent.setEnabled(enabled); }

        /**
         * @brief Check if input is enabled
         * @return True if input is enabled
         */
        bool isInputEnabled() const { return m_inputComponent.isEnabled(); }

    protected:
        /**
         * @brief Setup input bindings based on camera mode
         */
        virtual void setupInputBindings();

        /**
         * @brief Handle movement input
         * @param event Input event
         */
        virtual void handleMovementInput(const InputEvent& event);

        /**
         * @brief Handle mouse look input
         * @param event Input event
         */
        virtual void handleMouseLook(const InputEvent& event);

        /**
         * @brief Handle mouse scroll input (zoom, FOV adjustment)
         * @param event Input event
         */
        virtual void handleMouseScroll(const InputEvent& event);

        /**
         * @brief Update third-person camera position
         */
        void updateThirdPersonPosition();

    private:
        InputComponent m_inputComponent;    ///< Input component for handling controls
        CameraMode m_cameraMode;            ///< Current camera mode
        float m_movementSpeed;              ///< Movement speed (units per second)
        float m_mouseSensitivity;           ///< Mouse sensitivity
        
        // Third-person mode variables
        glm::vec3 m_target;                 ///< Target position for third-person mode
        float m_orbitDistance;              ///< Distance from target
        glm::vec2 m_orbitAngles;            ///< Orbit angles (pitch, yaw)
        
        // Movement state
        glm::vec2 m_rotation;               ///< Current rotation (pitch, yaw)
        bool m_movingForward;               ///< Forward movement state
        bool m_movingBackward;              ///< Backward movement state
        bool m_movingLeft;                  ///< Left movement state
        bool m_movingRight;                 ///< Right movement state
        bool m_movingUp;                    ///< Up movement state
        bool m_movingDown;                  ///< Down movement state
    };

    /**
     * @brief Interactive Light Entity with Input Component
     * 
     * This class extends TransformableLight to include input controls for
     * adjusting light properties and position.
     */
    class ControllableLight : public TransformableLight {
    public:
        /**
         * @brief Constructor
         * @param name Light name
         * @param type Light type
         * @param position Initial position
         * @param color Initial color
         * @param intensity Initial intensity
         */
        ControllableLight(const std::string& name,
                         LightType type = LightType::POINT,
                         const glm::vec3& position = glm::vec3(0.0f),
                         const glm::vec3& color = glm::vec3(1.0f),
                         float intensity = 1.0f);

        /**
         * @brief Virtual destructor
         */
        virtual ~ControllableLight() = default;

        /**
         * @brief Initialize the controllable light
         */
        virtual void initialize() override;

        /**
         * @brief Update the controllable light
         * @param deltaTime Time since last update
         */
        virtual void update(float deltaTime) override;

        /**
         * @brief Cleanup the controllable light
         */
        virtual void cleanup() override;

        /**
         * @brief Get the input component
         * @return Reference to input component
         */
        InputComponent& getInputComponent() { return m_inputComponent; }

        /**
         * @brief Get the input component (const version)
         * @return Const reference to input component
         */
        const InputComponent& getInputComponent() const { return m_inputComponent; }

        /**
         * @brief Set movement speed
         * @param speed Movement speed in units per second
         */
        void setMovementSpeed(float speed) { m_movementSpeed = speed; }

        /**
         * @brief Get movement speed
         * @return Current movement speed
         */
        float getMovementSpeed() const { return m_movementSpeed; }

        /**
         * @brief Enable/disable input processing
         * @param enabled True to enable input
         */
        void setInputEnabled(bool enabled) { m_inputComponent.setEnabled(enabled); }

        /**
         * @brief Check if input is enabled
         * @return True if input is enabled
         */
        bool isInputEnabled() const { return m_inputComponent.isEnabled(); }

    protected:
        /**
         * @brief Setup input bindings for light control
         */
        virtual void setupInputBindings();

        /**
         * @brief Handle movement input
         * @param event Input event
         */
        virtual void handleMovementInput(const InputEvent& event);

        /**
         * @brief Handle light property adjustment
         * @param event Input event
         */
        virtual void handleLightAdjustment(const InputEvent& event);

    private:
        InputComponent m_inputComponent;    ///< Input component for handling controls
        float m_movementSpeed;              ///< Movement speed (units per second)
        
        // Movement state
        bool m_movingForward;               ///< Forward movement state
        bool m_movingBackward;              ///< Backward movement state
        bool m_movingLeft;                  ///< Left movement state
        bool m_movingRight;                 ///< Right movement state
        bool m_movingUp;                    ///< Up movement state
        bool m_movingDown;                  ///< Down movement state
    };

} // namespace IKore
