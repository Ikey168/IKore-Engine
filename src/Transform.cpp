#include "Transform.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>

namespace IKore {

    // ============================================================================
    // Transform Implementation
    // ============================================================================

    Transform::Transform() 
        : m_position(0.0f)
        , m_rotation(0.0f)
        , m_scale(1.0f)
        , m_localMatrix(1.0f)
        , m_isDirty(true)
        , m_parent(nullptr) {
    }

    Transform::Transform(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
        : m_position(position)
        , m_rotation(rotation)
        , m_scale(scale)
        , m_localMatrix(1.0f)
        , m_isDirty(true)
        , m_parent(nullptr) {
    }

    Transform::Transform(const Transform& other)
        : m_position(other.m_position)
        , m_rotation(other.m_rotation)
        , m_scale(other.m_scale)
        , m_localMatrix(other.m_localMatrix)
        , m_isDirty(other.m_isDirty)
        , m_parent(nullptr) {
        // Note: We don't copy parent/child relationships in copy constructor
        // This prevents accidental hierarchy corruption
    }

    Transform::Transform(Transform&& other) noexcept
        : m_position(std::move(other.m_position))
        , m_rotation(std::move(other.m_rotation))
        , m_scale(std::move(other.m_scale))
        , m_localMatrix(std::move(other.m_localMatrix))
        , m_isDirty(other.m_isDirty)
        , m_parent(other.m_parent)
        , m_children(std::move(other.m_children)) {
        
        // Update parent's child reference
        if (m_parent) {
            auto& parentChildren = m_parent->m_children;
            auto it = std::find(parentChildren.begin(), parentChildren.end(), &other);
            if (it != parentChildren.end()) {
                *it = this;
            }
        }
        
        // Update children's parent references
        for (Transform* child : m_children) {
            child->m_parent = this;
        }
        
        // Clear moved-from object
        other.m_parent = nullptr;
        other.m_children.clear();
        other.m_isDirty = true;
    }

    Transform& Transform::operator=(const Transform& other) {
        if (this != &other) {
            // Remove from current hierarchy
            removeFromParent();
            for (Transform* child : m_children) {
                child->m_parent = nullptr;
            }
            m_children.clear();
            
            // Copy values
            m_position = other.m_position;
            m_rotation = other.m_rotation;
            m_scale = other.m_scale;
            m_localMatrix = other.m_localMatrix;
            m_isDirty = other.m_isDirty;
            m_parent = nullptr;
            
            // Note: We don't copy hierarchy relationships
        }
        return *this;
    }

    Transform& Transform::operator=(Transform&& other) noexcept {
        if (this != &other) {
            // Remove from current hierarchy
            removeFromParent();
            for (Transform* child : m_children) {
                child->m_parent = nullptr;
            }
            
            // Move values
            m_position = std::move(other.m_position);
            m_rotation = std::move(other.m_rotation);
            m_scale = std::move(other.m_scale);
            m_localMatrix = std::move(other.m_localMatrix);
            m_isDirty = other.m_isDirty;
            m_parent = other.m_parent;
            m_children = std::move(other.m_children);
            
            // Update parent's child reference
            if (m_parent) {
                auto& parentChildren = m_parent->m_children;
                auto it = std::find(parentChildren.begin(), parentChildren.end(), &other);
                if (it != parentChildren.end()) {
                    *it = this;
                }
            }
            
            // Update children's parent references
            for (Transform* child : m_children) {
                child->m_parent = this;
            }
            
            // Clear moved-from object
            other.m_parent = nullptr;
            other.m_children.clear();
            other.m_isDirty = true;
        }
        return *this;
    }

    Transform::~Transform() {
        // Remove from parent
        removeFromParent();
        
        // Clear children's parent references
        for (Transform* child : m_children) {
            if (child) {
                child->m_parent = nullptr;
            }
        }
    }

    // ============================================================================
    // Position Methods
    // ============================================================================

    void Transform::setPosition(const glm::vec3& position) {
        m_position = position;
        markDirty();
    }

    void Transform::setPosition(float x, float y, float z) {
        setPosition(glm::vec3(x, y, z));
    }

    void Transform::translate(const glm::vec3& translation) {
        m_position += translation;
        markDirty();
    }

    void Transform::translate(float x, float y, float z) {
        translate(glm::vec3(x, y, z));
    }

    glm::vec3 Transform::getWorldPosition() const {
        if (m_parent) {
            return glm::vec3(m_parent->getWorldMatrix() * glm::vec4(m_position, 1.0f));
        }
        return m_position;
    }

    // ============================================================================
    // Rotation Methods
    // ============================================================================

    void Transform::setRotation(const glm::vec3& rotation) {
        m_rotation = rotation;
        markDirty();
    }

    void Transform::setRotation(float x, float y, float z) {
        setRotation(glm::vec3(x, y, z));
    }

    void Transform::rotate(const glm::vec3& rotation) {
        m_rotation += rotation;
        markDirty();
    }

    void Transform::rotate(float x, float y, float z) {
        rotate(glm::vec3(x, y, z));
    }

    glm::quat Transform::getRotationQuaternion() const {
        return eulerToQuaternion(m_rotation);
    }

    void Transform::setRotationQuaternion(const glm::quat& quaternion) {
        m_rotation = quaternionToEuler(quaternion);
        markDirty();
    }

    glm::vec3 Transform::getWorldRotation() const {
        if (m_parent) {
            // For world rotation, we need to combine parent and local rotations
            glm::quat parentQuat = m_parent->getRotationQuaternion();
            glm::quat localQuat = getRotationQuaternion();
            glm::quat worldQuat = parentQuat * localQuat;
            return quaternionToEuler(worldQuat);
        }
        return m_rotation;
    }

    // ============================================================================
    // Scale Methods
    // ============================================================================

    void Transform::setScale(const glm::vec3& scale) {
        m_scale = scale;
        markDirty();
    }

    void Transform::setScale(float x, float y, float z) {
        setScale(glm::vec3(x, y, z));
    }

    void Transform::setScale(float scale) {
        setScale(glm::vec3(scale));
    }

    void Transform::scale(const glm::vec3& scaleFactor) {
        m_scale *= scaleFactor;
        markDirty();
    }

    void Transform::scale(float x, float y, float z) {
        scale(glm::vec3(x, y, z));
    }

    void Transform::scale(float scaleFactor) {
        scale(glm::vec3(scaleFactor));
    }

    glm::vec3 Transform::getWorldScale() const {
        if (m_parent) {
            return m_parent->getWorldScale() * m_scale;
        }
        return m_scale;
    }

    // ============================================================================
    // Matrix Methods
    // ============================================================================

    const glm::mat4& Transform::getLocalMatrix() const {
        if (m_isDirty) {
            calculateLocalMatrix();
            m_isDirty = false;
        }
        return m_localMatrix;
    }

    glm::mat4 Transform::getWorldMatrix() const {
        glm::mat4 localMatrix = getLocalMatrix();
        
        if (m_parent) {
            return m_parent->getWorldMatrix() * localMatrix;
        }
        
        return localMatrix;
    }

    void Transform::updateMatrices() {
        markDirty();
        // The matrices will be recalculated on next access
    }

    // ============================================================================
    // Hierarchy Methods
    // ============================================================================

    void Transform::setParent(Transform* parent) {
        if (m_parent == parent) {
            return; // No change
        }
        
        // Check for circular reference
        if (parent && parent->isChildOf(this)) {
            LOG_ERROR("Cannot set parent: would create circular reference in transform hierarchy");
            return;
        }
        
        // Remove from current parent
        removeFromParent();
        
        // Set new parent
        m_parent = parent;
        if (m_parent) {
            m_parent->addChild(this);
        }
        
        markDirty();
    }

    void Transform::addChild(Transform* child) {
        if (!child || child == this) {
            return;
        }
        
        // Check if already a child
        auto it = std::find(m_children.begin(), m_children.end(), child);
        if (it != m_children.end()) {
            return; // Already a child
        }
        
        // Check for circular reference
        if (isChildOf(child)) {
            LOG_ERROR("Cannot add child: would create circular reference in transform hierarchy");
            return;
        }
        
        m_children.push_back(child);
        
        // Update child's parent (this will handle removing from old parent)
        if (child->m_parent != this) {
            child->setParent(this);
        }
    }

    void Transform::removeChild(Transform* child) {
        if (!child) {
            return;
        }
        
        auto it = std::find(m_children.begin(), m_children.end(), child);
        if (it != m_children.end()) {
            m_children.erase(it);
            child->m_parent = nullptr;
            child->markDirty();
        }
    }

    bool Transform::isChildOf(const Transform* potentialParent) const {
        if (!potentialParent) {
            return false;
        }
        
        const Transform* current = m_parent;
        while (current) {
            if (current == potentialParent) {
                return true;
            }
            current = current->m_parent;
        }
        
        return false;
    }

    Transform* Transform::getRoot() {
        Transform* current = this;
        while (current->m_parent) {
            current = current->m_parent;
        }
        return current;
    }

    int Transform::getHierarchyDepth() const {
        int depth = 0;
        const Transform* current = m_parent;
        while (current) {
            depth++;
            current = current->m_parent;
        }
        return depth;
    }

    // ============================================================================
    // Utility Methods
    // ============================================================================

    glm::vec3 Transform::getForward() const {
        glm::mat4 worldMatrix = getWorldMatrix();
        return -glm::normalize(glm::vec3(worldMatrix[2])); // -Z axis
    }

    glm::vec3 Transform::getRight() const {
        glm::mat4 worldMatrix = getWorldMatrix();
        return glm::normalize(glm::vec3(worldMatrix[0])); // +X axis
    }

    glm::vec3 Transform::getUp() const {
        glm::mat4 worldMatrix = getWorldMatrix();
        return glm::normalize(glm::vec3(worldMatrix[1])); // +Y axis
    }

    void Transform::lookAt(const glm::vec3& target, const glm::vec3& up) {
        glm::vec3 worldPos = getWorldPosition();
        glm::vec3 direction = glm::normalize(target - worldPos);
        
        // Calculate rotation to look at target
        glm::mat4 lookAtMatrix = glm::lookAt(worldPos, target, up);
        glm::mat4 rotationMatrix = glm::inverse(lookAtMatrix);
        
        // Extract rotation from matrix and convert to Euler angles
        glm::quat rotation = glm::quat_cast(rotationMatrix);
        setRotation(quaternionToEuler(rotation));
    }

    void Transform::reset() {
        m_position = glm::vec3(0.0f);
        m_rotation = glm::vec3(0.0f);
        m_scale = glm::vec3(1.0f);
        markDirty();
    }

    void Transform::forEachInHierarchy(const std::function<void(Transform*)>& func) {
        if (!func) {
            return;
        }
        
        func(this);
        for (Transform* child : m_children) {
            if (child) {
                child->forEachInHierarchy(func);
            }
        }
    }

    // ============================================================================
    // Static Utility Methods
    // ============================================================================

    glm::mat4 Transform::createMatrix(const glm::vec3& position, 
                                     const glm::vec3& rotation, 
                                     const glm::vec3& scale) {
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
        
        glm::mat4 rotationMatrix = glm::eulerAngleYXZ(
            glm::radians(rotation.y), // Yaw
            glm::radians(rotation.x), // Pitch
            glm::radians(rotation.z)  // Roll
        );
        
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);
        
        return translationMatrix * rotationMatrix * scaleMatrix;
    }

    glm::quat Transform::eulerToQuaternion(const glm::vec3& eulerDegrees) {
        glm::vec3 eulerRadians = glm::radians(eulerDegrees);
        return glm::quat(eulerRadians);
    }

    glm::vec3 Transform::quaternionToEuler(const glm::quat& quaternion) {
        glm::vec3 eulerRadians = glm::eulerAngles(quaternion);
        return glm::degrees(eulerRadians);
    }

    // ============================================================================
    // Private Methods
    // ============================================================================

    void Transform::markDirty() {
        m_isDirty = true;
        
        // Mark all children as dirty too
        for (Transform* child : m_children) {
            if (child) {
                child->markDirty();
            }
        }
    }

    void Transform::calculateLocalMatrix() const {
        m_localMatrix = createMatrix(m_position, m_rotation, m_scale);
    }

    void Transform::removeFromParent() {
        if (m_parent) {
            m_parent->removeChild(this);
        }
    }

    // ============================================================================
    // TransformComponent Implementation
    // ============================================================================

    TransformComponent::TransformComponent() {
    }

    TransformComponent::TransformComponent(const glm::vec3& position, 
                                         const glm::vec3& rotation, 
                                         const glm::vec3& scale)
        : m_transform(position, rotation, scale) {
    }

    void TransformComponent::update(float deltaTime) {
        // TransformComponent doesn't need per-frame updates by default
        // Subclasses can override this for custom behavior
        (void)deltaTime; // Suppress unused parameter warning
    }

    void TransformComponent::initialize() {
        // Initialize the transform component
        // Default implementation does nothing
    }

    void TransformComponent::cleanup() {
        // Cleanup the transform component
        // Default implementation does nothing
    }

} // namespace IKore
