#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace IKore {

    /**
     * @brief Input action type identifiers
     */
    enum class InputActionType {
        PRESS,      ///< Key/button was just pressed this frame
        RELEASE,    ///< Key/button was just released this frame
        HOLD,       ///< Key/button is being held down
        REPEAT      ///< Key/button is repeating (for text input)
    };

    /**
     * @brief Mouse button identifiers
     */
    enum class MouseButton {
        LEFT = GLFW_MOUSE_BUTTON_LEFT,
        RIGHT = GLFW_MOUSE_BUTTON_RIGHT,
        MIDDLE = GLFW_MOUSE_BUTTON_MIDDLE,
        BUTTON_4 = GLFW_MOUSE_BUTTON_4,
        BUTTON_5 = GLFW_MOUSE_BUTTON_5,
        BUTTON_6 = GLFW_MOUSE_BUTTON_6,
        BUTTON_7 = GLFW_MOUSE_BUTTON_7,
        BUTTON_8 = GLFW_MOUSE_BUTTON_8
    };

    /**
     * @brief Input event type identifiers
     */
    enum class InputEventType {
        KEY_PRESS,      ///< Key was pressed
        KEY_RELEASE,    ///< Key was released
        KEY_REPEAT,     ///< Key is repeating
        MOUSE_MOVE,     ///< Mouse moved
        MOUSE_PRESS,    ///< Mouse button pressed
        MOUSE_RELEASE,  ///< Mouse button released
        MOUSE_SCROLL    ///< Mouse scroll wheel
    };

    /**
     * @brief Input event data structure
     */
    struct InputEvent {
        InputEventType type;        ///< Type of input event
        int key;                    ///< Key code (for keyboard events)
        MouseButton mouseButton;   ///< Mouse button (for mouse events)
        InputActionType action;     ///< Action type
        float deltaTime;           ///< Time since last frame
        glm::vec2 mousePosition;   ///< Current mouse position
        glm::vec2 mouseDelta;      ///< Mouse movement delta
        float scrollDelta;         ///< Mouse scroll wheel delta
        
        // Additional data structures for easier access
        struct {
            float deltaX, deltaY;
        } mouseData;
        
        struct {
            float yOffset;
        } scrollData;
    };

    /**
     * @brief Callback function type for input actions
     */
    using InputCallback = std::function<void(const InputEvent&)>;

    /**
     * @brief Input action binding structure
     */
    struct InputBinding {
        std::string actionName;     ///< Name of the action
        int key;                   ///< Key code (GLFW_KEY_*)
        MouseButton mouseButton;   ///< Mouse button
        InputActionType actionType; ///< When to trigger (press, hold, etc.)
        InputCallback callback;    ///< Function to call when triggered
        bool isMouseAction;        ///< True if this is a mouse action
        float repeatDelay;         ///< Delay for repeat actions (seconds)
        float lastTriggerTime;     ///< Last time this action was triggered
    };

    /**
     * @brief Input Manager - Singleton class for managing all input
     * 
     * This class handles GLFW input callbacks and distributes input events
     * to registered InputComponents. It provides a centralized input system
     * with minimal lag and efficient event distribution.
     */
    class InputManager {
    public:
        /**
         * @brief Get the singleton instance
         * @return Reference to the InputManager instance
         */
        static InputManager& getInstance();

        /**
         * @brief Initialize the input manager with a GLFW window
         * @param window GLFW window pointer
         */
        void initialize(GLFWwindow* window);

        /**
         * @brief Shutdown the input manager
         */
        void shutdown();

        /**
         * @brief Update the input manager (call once per frame)
         * @param deltaTime Time since last frame
         */
        void update(float deltaTime);

        /**
         * @brief Register an input binding
         * @param binding Input binding to register
         * @return Unique ID for the binding (for removal)
         */
        int registerBinding(const InputBinding& binding);

        /**
         * @brief Unregister an input binding
         * @param bindingId ID of the binding to remove
         */
        void unregisterBinding(int bindingId);

        /**
         * @brief Clear all input bindings
         */
        void clearAllBindings();

        /**
         * @brief Check if a key is currently pressed
         * @param key Key code (GLFW_KEY_*)
         * @return True if the key is pressed
         */
        bool isKeyPressed(int key) const;

        /**
         * @brief Check if a key was just pressed this frame
         * @param key Key code (GLFW_KEY_*)
         * @return True if the key was just pressed
         */
        bool isKeyJustPressed(int key) const;

        /**
         * @brief Check if a key was just released this frame
         * @param key Key code (GLFW_KEY_*)
         * @return True if the key was just released
         */
        bool isKeyJustReleased(int key) const;

        /**
         * @brief Check if a mouse button is currently pressed
         * @param button Mouse button
         * @return True if the button is pressed
         */
        bool isMouseButtonPressed(MouseButton button) const;

        /**
         * @brief Check if a mouse button was just pressed this frame
         * @param button Mouse button
         * @return True if the button was just pressed
         */
        bool isMouseButtonJustPressed(MouseButton button) const;

        /**
         * @brief Check if a mouse button was just released this frame
         * @param button Mouse button
         * @return True if the button was just released
         */
        bool isMouseButtonJustReleased(MouseButton button) const;

        /**
         * @brief Get current mouse position
         * @return Mouse position in screen coordinates
         */
        glm::vec2 getMousePosition() const { return m_mousePosition; }

        /**
         * @brief Get mouse movement delta since last frame
         * @return Mouse delta in pixels
         */
        glm::vec2 getMouseDelta() const { return m_mouseDelta; }

        /**
         * @brief Get mouse scroll wheel delta
         * @return Scroll delta (positive = up, negative = down)
         */
        float getScrollDelta() const { return m_scrollDelta; }

        /**
         * @brief Set mouse cursor mode
         * @param mode GLFW cursor mode (GLFW_CURSOR_NORMAL, GLFW_CURSOR_DISABLED, etc.)
         */
        void setCursorMode(int mode);

        /**
         * @brief Get current cursor mode
         * @return Current GLFW cursor mode
         */
        int getCursorMode() const;

        /**
         * @brief Enable/disable mouse capture for camera-style look controls
         * @param enable True to enable mouse capture
         */
        void setMouseCapture(bool enable);

        /**
         * @brief Check if mouse is captured
         * @return True if mouse is captured
         */
        bool isMouseCaptured() const { return m_mouseCaptured; }

        /**
         * @brief Set mouse sensitivity for look controls
         * @param sensitivity Mouse sensitivity multiplier
         */
        void setMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }

        /**
         * @brief Get mouse sensitivity
         * @return Current mouse sensitivity
         */
        float getMouseSensitivity() const { return m_mouseSensitivity; }

    private:
        InputManager() = default;
        ~InputManager() = default;
        InputManager(const InputManager&) = delete;
        InputManager& operator=(const InputManager&) = delete;

        /**
         * @brief Process keyboard input
         * @param key Key code
         * @param action GLFW action (press, release, repeat)
         */
        void processKeyboardInput(int key, int action);

        /**
         * @brief Process mouse button input
         * @param button Mouse button
         * @param action GLFW action (press, release)
         */
        void processMouseButtonInput(int button, int action);

        /**
         * @brief Process mouse movement
         * @param xpos Mouse X position
         * @param ypos Mouse Y position
         */
        void processMouseMovement(double xpos, double ypos);

        /**
         * @brief Process mouse scroll
         * @param xoffset Horizontal scroll offset
         * @param yoffset Vertical scroll offset
         */
        void processMouseScroll(double xoffset, double yoffset);

        /**
         * @brief Trigger input bindings for a key event
         * @param key Key code
         * @param actionType Action type
         */
        void triggerKeyBindings(int key, InputActionType actionType);

        /**
         * @brief Trigger input bindings for a mouse event
         * @param button Mouse button
         * @param actionType Action type
         */
        void triggerMouseBindings(MouseButton button, InputActionType actionType);

        // GLFW callback functions (static)
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
        static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

        // Member variables
        GLFWwindow* m_window;                                           ///< GLFW window pointer
        bool m_initialized;                                             ///< Initialization flag
        
        // Input state tracking
        std::unordered_map<int, bool> m_keyStates;                      ///< Current key states
        std::unordered_map<int, bool> m_previousKeyStates;              ///< Previous frame key states
        std::unordered_map<int, bool> m_mouseButtonStates;              ///< Current mouse button states
        std::unordered_map<int, bool> m_previousMouseButtonStates;      ///< Previous frame mouse button states
        
        // Mouse state
        glm::vec2 m_mousePosition;                                      ///< Current mouse position
        glm::vec2 m_previousMousePosition;                              ///< Previous mouse position
        glm::vec2 m_mouseDelta;                                         ///< Mouse movement delta
        float m_scrollDelta;                                            ///< Mouse scroll delta
        bool m_mouseCaptured;                                           ///< Mouse capture state
        float m_mouseSensitivity;                                       ///< Mouse sensitivity
        bool m_firstMouseMovement;                                      ///< Flag for first mouse movement
        
        // Input bindings
        std::unordered_map<int, InputBinding> m_bindings;               ///< Registered input bindings
        int m_nextBindingId;                                            ///< Next available binding ID
        float m_currentTime;                                            ///< Current time for repeat actions
    };

    /**
     * @brief Input Component for entities
     * 
     * This component allows entities to register input callbacks and respond
     * to keyboard and mouse events. It integrates with the InputManager for
     * efficient input handling with minimal lag.
     */
    class InputComponent {
    public:
        /**
         * @brief Constructor
         */
        InputComponent();

        /**
         * @brief Constructor with name
         * @param name Component name for debugging
         */
        InputComponent(const std::string& name);

        /**
         * @brief Destructor - automatically unregisters all bindings
         */
        ~InputComponent();

        /**
         * @brief Copy constructor
         */
        InputComponent(const InputComponent& other);

        /**
         * @brief Move constructor
         */
        InputComponent(InputComponent&& other) noexcept;

        /**
         * @brief Copy assignment operator
         */
        InputComponent& operator=(const InputComponent& other);

        /**
         * @brief Move assignment operator
         */
        InputComponent& operator=(InputComponent&& other) noexcept;

        /**
         * @brief Initialize the input component
         */
        void initialize();

        /**
         * @brief Update the input component
         * @param deltaTime Time since last update
         */
        void update(float deltaTime);

        /**
         * @brief Cleanup the input component
         */
        void cleanup();

        /**
         * @brief Bind a keyboard action
         * @param actionName Name of the action
         * @param key Key code (GLFW_KEY_*)
         * @param actionType When to trigger (press, hold, etc.)
         * @param callback Function to call when triggered
         * @param repeatDelay Delay for repeat actions (default: 0.1 seconds)
         * @return Binding ID for removal
         */
        int bindKeyAction(const std::string& actionName, int key, InputActionType actionType, 
                         InputCallback callback, float repeatDelay = 0.1f);

        /**
         * @brief Bind a mouse button action
         * @param actionName Name of the action
         * @param button Mouse button
         * @param actionType When to trigger (press, hold, etc.)
         * @param callback Function to call when triggered
         * @return Binding ID for removal
         */
        int bindMouseAction(const std::string& actionName, MouseButton button, InputActionType actionType,
                           InputCallback callback);

        /**
         * @brief Bind mouse movement action
         * @param actionName Name of the action
         * @param callback Function to call when mouse moves
         * @return Binding ID for removal
         */
        int bindMouseMovement(const std::string& actionName, InputCallback callback);

        /**
         * @brief Bind mouse scroll action
         * @param actionName Name of the action
         * @param callback Function to call when mouse scrolls
         * @return Binding ID for removal
         */
        int bindMouseScroll(const std::string& actionName, InputCallback callback);

        /**
         * @brief Unbind an action by ID
         * @param bindingId ID of the binding to remove
         */
        void unbindAction(int bindingId);

        /**
         * @brief Unbind all actions with a specific name
         * @param actionName Name of actions to remove
         */
        void unbindAction(const std::string& actionName);

        /**
         * @brief Clear all input bindings
         */
        void clearAllBindings();

        /**
         * @brief Simplified key binding (press/release/repeat)
         * @param key Key code (GLFW_KEY_*)
         * @param callback Function to call when key state changes
         * @return Binding ID for removal
         */
        int bindKeyAction(int key, InputCallback callback);

        /**
         * @brief Simplified mouse move binding
         * @param callback Function to call on mouse movement
         * @return Binding ID for removal
         */
        int bindMouseMoveAction(InputCallback callback);

        /**
         * @brief Simplified scroll binding
         * @param callback Function to call on scroll
         * @return Binding ID for removal
         */
        int bindScrollAction(InputCallback callback);

        /**
         * @brief Clear all bindings (alias for clearAllBindings)
         */
        void clearBindings() { clearAllBindings(); }

        /**
         * @brief Check if this component has any bindings
         * @return True if there are active bindings
         */
        bool hasBindings() const { return !m_bindingIds.empty(); }

        /**
         * @brief Get the number of active bindings
         * @return Number of bindings
         */
        size_t getBindingCount() const { return m_bindingIds.size(); }

        /**
         * @brief Enable/disable this input component
         * @param enabled True to enable input processing
         */
        void setEnabled(bool enabled) { m_enabled = enabled; }

        /**
         * @brief Check if this input component is enabled
         * @return True if enabled
         */
        bool isEnabled() const { return m_enabled; }

    private:
        /**
         * @brief Copy bindings from another component
         * @param other Source component
         */
        void copyBindingsFrom(const InputComponent& other);

        std::vector<int> m_bindingIds;          ///< IDs of registered bindings
        std::vector<std::string> m_actionNames; ///< Names of registered actions
        bool m_enabled;                         ///< Enable/disable flag
        bool m_initialized;                     ///< Initialization flag
    };

    /**
     * @brief Convenience function to get the InputManager instance
     * @return Reference to InputManager singleton
     */
    inline InputManager& getInputManager() {
        return InputManager::getInstance();
    }

} // namespace IKore
