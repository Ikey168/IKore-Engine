#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <memory>
#include <vector>
#include <functional>

namespace IKore {

    /**
     * @brief TransformComponent handles 3D transformations including position, rotation, and scale.
     * 
     * This component provides a hierarchical transformation system where transforms can have
     * parent-child relationships. All transformations are calculated relative to the parent.
     * 
     * Features:
     * - Position, Rotation (Euler angles), and Scale storage
     * - Hierarchical parent-child relationships
     * - Local and world space transformation matrices
     * - Automatic matrix recalculation when dirty
     * - Easy manipulation methods for common operations
     */
    class Transform {
    public:
        // ============================================================================
        // Constructors and Destructor
        // ============================================================================
        
        /**
         * @brief Default constructor - creates identity transform
         */
        Transform();
        
        /**
         * @brief Constructor with initial values
         * @param position Initial position (default: origin)
         * @param rotation Initial rotation in degrees (default: no rotation)
         * @param scale Initial scale (default: unit scale)
         */
        Transform(const glm::vec3& position, 
                 const glm::vec3& rotation = glm::vec3(0.0f), 
                 const glm::vec3& scale = glm::vec3(1.0f));
        
        /**
         * @brief Copy constructor
         */
        Transform(const Transform& other);
        
        /**
         * @brief Move constructor
         */
        Transform(Transform&& other) noexcept;
        
        /**
         * @brief Copy assignment operator
         */
        Transform& operator=(const Transform& other);
        
        /**
         * @brief Move assignment operator
         */
        Transform& operator=(Transform&& other) noexcept;
        
        /**
         * @brief Destructor
         */
        ~Transform();

        // ============================================================================
        // Position Methods
        // ============================================================================
        
        /**
         * @brief Get the local position
         * @return Local position vector
         */
        const glm::vec3& getPosition() const { return m_position; }
        
        /**
         * @brief Set the local position
         * @param position New local position
         */
        void setPosition(const glm::vec3& position);
        
        /**
         * @brief Set the local position using individual components
         * @param x X component
         * @param y Y component
         * @param z Z component
         */
        void setPosition(float x, float y, float z);
        
        /**
         * @brief Translate by a vector (add to current position)
         * @param translation Translation vector
         */
        void translate(const glm::vec3& translation);
        
        /**
         * @brief Translate by individual components
         * @param x X translation
         * @param y Y translation
         * @param z Z translation
         */
        void translate(float x, float y, float z);
        
        /**
         * @brief Get the world position (considering parent hierarchy)
         * @return World position vector
         */
        glm::vec3 getWorldPosition() const;

        // ============================================================================
        // Rotation Methods
        // ============================================================================
        
        /**
         * @brief Get the local rotation in degrees
         * @return Local rotation vector (Euler angles in degrees)
         */
        const glm::vec3& getRotation() const { return m_rotation; }
        
        /**
         * @brief Set the local rotation using Euler angles in degrees
         * @param rotation Rotation vector (x=pitch, y=yaw, z=roll in degrees)
         */
        void setRotation(const glm::vec3& rotation);
        
        /**
         * @brief Set the local rotation using individual components
         * @param x Pitch in degrees
         * @param y Yaw in degrees
         * @param z Roll in degrees
         */
        void setRotation(float x, float y, float z);
        
        /**
         * @brief Rotate by Euler angles (add to current rotation)
         * @param rotation Additional rotation in degrees
         */
        void rotate(const glm::vec3& rotation);
        
        /**
         * @brief Rotate by individual components
         * @param x Pitch rotation in degrees
         * @param y Yaw rotation in degrees
         * @param z Roll rotation in degrees
         */
        void rotate(float x, float y, float z);
        
        /**
         * @brief Get the local rotation as a quaternion
         * @return Rotation quaternion
         */
        glm::quat getRotationQuaternion() const;
        
        /**
         * @brief Set rotation using a quaternion
         * @param quaternion Rotation quaternion
         */
        void setRotationQuaternion(const glm::quat& quaternion);
        
        /**
         * @brief Get the world rotation (considering parent hierarchy)
         * @return World rotation vector (Euler angles in degrees)
         */
        glm::vec3 getWorldRotation() const;

        // ============================================================================
        // Scale Methods
        // ============================================================================
        
        /**
         * @brief Get the local scale
         * @return Local scale vector
         */
        const glm::vec3& getScale() const { return m_scale; }
        
        /**
         * @brief Set the local scale
         * @param scale New local scale
         */
        void setScale(const glm::vec3& scale);
        
        /**
         * @brief Set the local scale using individual components
         * @param x X scale
         * @param y Y scale
         * @param z Z scale
         */
        void setScale(float x, float y, float z);
        
        /**
         * @brief Set uniform scale (same for all axes)
         * @param scale Uniform scale value
         */
        void setScale(float scale);
        
        /**
         * @brief Scale by a factor (multiply current scale)
         * @param scaleFactor Scale factor vector
         */
        void scale(const glm::vec3& scaleFactor);
        
        /**
         * @brief Scale by individual factors
         * @param x X scale factor
         * @param y Y scale factor
         * @param z Z scale factor
         */
        void scale(float x, float y, float z);
        
        /**
         * @brief Scale uniformly by a factor
         * @param scaleFactor Uniform scale factor
         */
        void scale(float scaleFactor);
        
        /**
         * @brief Get the world scale (considering parent hierarchy)
         * @return World scale vector
         */
        glm::vec3 getWorldScale() const;

        // ============================================================================
        // Matrix Methods
        // ============================================================================
        
        /**
         * @brief Get the local transformation matrix
         * @return Local transformation matrix
         */
        const glm::mat4& getLocalMatrix() const;
        
        /**
         * @brief Get the world transformation matrix (considering parent hierarchy)
         * @return World transformation matrix
         */
        glm::mat4 getWorldMatrix() const;
        
        /**
         * @brief Force recalculation of transformation matrices
         */
        void updateMatrices();

        // ============================================================================
        // Hierarchy Methods
        // ============================================================================
        
        /**
         * @brief Set the parent transform
         * @param parent Pointer to parent transform (nullptr for no parent)
         */
        void setParent(Transform* parent);
        
        /**
         * @brief Get the parent transform
         * @return Pointer to parent transform (nullptr if no parent)
         */
        Transform* getParent() const { return m_parent; }
        
        /**
         * @brief Add a child transform
         * @param child Pointer to child transform
         */
        void addChild(Transform* child);
        
        /**
         * @brief Remove a child transform
         * @param child Pointer to child transform to remove
         */
        void removeChild(Transform* child);
        
        /**
         * @brief Get all child transforms
         * @return Vector of child transform pointers
         */
        const std::vector<Transform*>& getChildren() const { return m_children; }
        
        /**
         * @brief Check if this transform is a child of another transform
         * @param potentialParent Transform to check against
         * @return True if this is a child (direct or indirect) of potentialParent
         */
        bool isChildOf(const Transform* potentialParent) const;
        
        /**
         * @brief Get the root transform (top of hierarchy)
         * @return Pointer to root transform
         */
        Transform* getRoot();
        
        /**
         * @brief Get the depth in the hierarchy (0 = root)
         * @return Hierarchy depth
         */
        int getHierarchyDepth() const;

        // ============================================================================
        // Utility Methods
        // ============================================================================
        
        /**
         * @brief Get the forward direction vector (local -Z axis)
         * @return Forward direction vector in world space
         */
        glm::vec3 getForward() const;
        
        /**
         * @brief Get the right direction vector (local +X axis)
         * @return Right direction vector in world space
         */
        glm::vec3 getRight() const;
        
        /**
         * @brief Get the up direction vector (local +Y axis)
         * @return Up direction vector in world space
         */
        glm::vec3 getUp() const;
        
        /**
         * @brief Look at a target position
         * @param target Target position to look at
         * @param up Up vector (default: world Y-axis)
         */
        void lookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));
        
        /**
         * @brief Reset to identity transform
         */
        void reset();
        
        /**
         * @brief Check if the transform has been modified since last matrix update
         * @return True if transform is dirty
         */
        bool isDirty() const { return m_isDirty; }
        
        /**
         * @brief Apply a function to this transform and all its children recursively
         * @param func Function to apply
         */
        void forEachInHierarchy(const std::function<void(Transform*)>& func);

        // ============================================================================
        // Static Utility Methods
        // ============================================================================
        
        /**
         * @brief Create a transformation matrix from position, rotation, and scale
         * @param position Position vector
         * @param rotation Rotation vector (Euler angles in degrees)
         * @param scale Scale vector
         * @return Transformation matrix
         */
        static glm::mat4 createMatrix(const glm::vec3& position, 
                                     const glm::vec3& rotation, 
                                     const glm::vec3& scale);
        
        /**
         * @brief Convert Euler angles to quaternion
         * @param eulerDegrees Euler angles in degrees (x=pitch, y=yaw, z=roll)
         * @return Quaternion representation
         */
        static glm::quat eulerToQuaternion(const glm::vec3& eulerDegrees);
        
        /**
         * @brief Convert quaternion to Euler angles
         * @param quaternion Quaternion to convert
         * @return Euler angles in degrees (x=pitch, y=yaw, z=roll)
         */
        static glm::vec3 quaternionToEuler(const glm::quat& quaternion);

    private:
        // ============================================================================
        // Private Methods
        // ============================================================================
        
        /**
         * @brief Mark the transform as dirty and propagate to children
         */
        void markDirty();
        
        /**
         * @brief Recalculate the local transformation matrix
         */
        void calculateLocalMatrix() const;
        
        /**
         * @brief Remove this transform from its parent's children list
         */
        void removeFromParent();

        // ============================================================================
        // Member Variables
        // ============================================================================
        
        // Transformation components
        glm::vec3 m_position;      ///< Local position
        glm::vec3 m_rotation;      ///< Local rotation (Euler angles in degrees)
        glm::vec3 m_scale;         ///< Local scale
        
        // Matrix cache
        mutable glm::mat4 m_localMatrix;  ///< Cached local transformation matrix
        mutable bool m_isDirty;           ///< Flag indicating if matrices need recalculation
        
        // Hierarchy
        Transform* m_parent;                    ///< Parent transform (nullptr if root)
        std::vector<Transform*> m_children;    ///< Child transforms
    };

    // ============================================================================
    // TransformComponent for Entity System Integration
    // ============================================================================
    
    /**
     * @brief Component wrapper for Transform to integrate with Entity system
     * 
     * This class provides a component interface for the Transform class,
     * allowing it to be easily attached to entities in the Entity system.
     */
    class TransformComponent {
    public:
        /**
         * @brief Default constructor
         */
        TransformComponent();
        
        /**
         * @brief Constructor with initial transform values
         * @param position Initial position
         * @param rotation Initial rotation in degrees
         * @param scale Initial scale
         */
        TransformComponent(const glm::vec3& position, 
                          const glm::vec3& rotation = glm::vec3(0.0f), 
                          const glm::vec3& scale = glm::vec3(1.0f));
        
        /**
         * @brief Get the underlying transform
         * @return Reference to the transform
         */
        Transform& getTransform() { return m_transform; }
        
        /**
         * @brief Get the underlying transform (const version)
         * @return Const reference to the transform
         */
        const Transform& getTransform() const { return m_transform; }
        
        /**
         * @brief Update the component (called per frame)
         * @param deltaTime Time since last update
         */
        void update(float deltaTime);
        
        /**
         * @brief Initialize the component
         */
        void initialize();
        
        /**
         * @brief Cleanup the component
         */
        void cleanup();

    private:
        Transform m_transform;  ///< The actual transform object
    };

} // namespace IKore
