#include "PhysicsComponent.h"
#include "../Entity.h"
#include "TransformComponent.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace IKore {

    PhysicsComponent::PhysicsComponent() {
        // Constructor - initialization happens in initializeRigidBody
    }

    PhysicsComponent::~PhysicsComponent() {
        // Bullet objects are automatically cleaned up by unique_ptr
        // The physics system should remove the rigid body from the world before destruction
    }

    void PhysicsComponent::initializeRigidBody(BodyType bodyType, ColliderType colliderType, 
                                             const glm::vec3& dimensions, float mass) {
        m_bodyType = bodyType;
        m_colliderType = colliderType;
        m_dimensions = dimensions;
        m_mass = (bodyType == BodyType::STATIC) ? 0.0f : mass;

        createCollisionShape(colliderType, dimensions);
        setupRigidBody(m_mass);
    }

    void PhysicsComponent::initializeRigidBody(BodyType bodyType, std::unique_ptr<btCollisionShape> shape, float mass) {
        m_bodyType = bodyType;
        m_colliderType = ColliderType::MESH; // Custom shape
        m_mass = (bodyType == BodyType::STATIC) ? 0.0f : mass;
        m_collisionShape = std::move(shape);

        setupRigidBody(m_mass);
    }

    void PhysicsComponent::createCollisionShape(ColliderType type, const glm::vec3& dimensions) {
        switch (type) {
            case ColliderType::BOX: {
                btVector3 halfExtents(dimensions.x * 0.5f, dimensions.y * 0.5f, dimensions.z * 0.5f);
                m_collisionShape = std::make_unique<btBoxShape>(halfExtents);
                break;
            }
            case ColliderType::SPHERE: {
                float radius = dimensions.x; // Use x component as radius
                m_collisionShape = std::make_unique<btSphereShape>(radius);
                break;
            }
            case ColliderType::CAPSULE: {
                float radius = dimensions.x; // x = radius
                float height = dimensions.y; // y = height
                m_collisionShape = std::make_unique<btCapsuleShape>(radius, height);
                break;
            }
            case ColliderType::PLANE: {
                btVector3 normal = glmToBullet(glm::normalize(dimensions));
                m_collisionShape = std::make_unique<btStaticPlaneShape>(normal, 0.0f);
                break;
            }
            case ColliderType::MESH: {
                // For mesh colliders, this should be handled by the custom shape overload
                // Default to box for safety
                btVector3 halfExtents(0.5f, 0.5f, 0.5f);
                m_collisionShape = std::make_unique<btBoxShape>(halfExtents);
                break;
            }
        }
    }

    void PhysicsComponent::setupRigidBody(float mass) {
        // Calculate local inertia
        btVector3 localInertia(0, 0, 0);
        if (mass > 0.0f) {
            m_collisionShape->calculateLocalInertia(mass, localInertia);
        }

        // Get initial transform from entity's transform component
        btTransform startTransform;
        startTransform.setIdentity();
        
        if (auto entity = getEntity().lock()) {
            if (auto transform = entity->getComponent<TransformComponent>()) {
                startTransform.setOrigin(glmToBullet(transform->position));
                
                // Convert Euler angles to quaternion
                glm::quat rotation = glm::quat(glm::radians(transform->rotation));
                startTransform.setRotation(glmToBullet(rotation));
            }
        }

        // Create motion state
        m_motionState = std::make_unique<btDefaultMotionState>(startTransform);

        // Create rigid body
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, m_motionState.get(), m_collisionShape.get(), localInertia);
        m_rigidBody = std::make_unique<btRigidBody>(rbInfo);

        // Set body type specific properties
        if (m_bodyType == BodyType::KINEMATIC) {
            m_rigidBody->setCollisionFlags(m_rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            m_rigidBody->setActivationState(DISABLE_DEACTIVATION);
        } else if (m_bodyType == BodyType::STATIC) {
            m_rigidBody->setCollisionFlags(m_rigidBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
        }

        // Set default material properties
        m_rigidBody->setFriction(0.5f);
        m_rigidBody->setRestitution(0.3f);
        m_rigidBody->setDamping(0.1f, 0.1f);
    }

    // Force and impulse application
    void PhysicsComponent::applyForce(const glm::vec3& force) {
        if (m_rigidBody && isDynamic()) {
            m_rigidBody->activate();
            m_rigidBody->applyCentralForce(glmToBullet(force));
        }
    }

    void PhysicsComponent::applyForce(const glm::vec3& force, const glm::vec3& relativePos) {
        if (m_rigidBody && isDynamic()) {
            m_rigidBody->activate();
            m_rigidBody->applyForce(glmToBullet(force), glmToBullet(relativePos));
        }
    }

    void PhysicsComponent::applyImpulse(const glm::vec3& impulse) {
        if (m_rigidBody && isDynamic()) {
            m_rigidBody->activate();
            m_rigidBody->applyCentralImpulse(glmToBullet(impulse));
        }
    }

    void PhysicsComponent::applyImpulse(const glm::vec3& impulse, const glm::vec3& relativePos) {
        if (m_rigidBody && isDynamic()) {
            m_rigidBody->activate();
            m_rigidBody->applyImpulse(glmToBullet(impulse), glmToBullet(relativePos));
        }
    }

    void PhysicsComponent::applyTorque(const glm::vec3& torque) {
        if (m_rigidBody && isDynamic()) {
            m_rigidBody->activate();
            m_rigidBody->applyTorque(glmToBullet(torque));
        }
    }

    void PhysicsComponent::applyTorqueImpulse(const glm::vec3& torque) {
        if (m_rigidBody && isDynamic()) {
            m_rigidBody->activate();
            m_rigidBody->applyTorqueImpulse(glmToBullet(torque));
        }
    }

    // Velocity control
    void PhysicsComponent::setLinearVelocity(const glm::vec3& velocity) {
        if (m_rigidBody) {
            m_rigidBody->setLinearVelocity(glmToBullet(velocity));
            if (velocity != glm::vec3(0.0f)) {
                m_rigidBody->activate();
            }
        }
    }

    void PhysicsComponent::setAngularVelocity(const glm::vec3& velocity) {
        if (m_rigidBody) {
            m_rigidBody->setAngularVelocity(glmToBullet(velocity));
            if (velocity != glm::vec3(0.0f)) {
                m_rigidBody->activate();
            }
        }
    }

    glm::vec3 PhysicsComponent::getLinearVelocity() const {
        if (m_rigidBody) {
            return bulletToGlm(m_rigidBody->getLinearVelocity());
        }
        return glm::vec3(0.0f);
    }

    glm::vec3 PhysicsComponent::getAngularVelocity() const {
        if (m_rigidBody) {
            return bulletToGlm(m_rigidBody->getAngularVelocity());
        }
        return glm::vec3(0.0f);
    }

    // Position and rotation
    void PhysicsComponent::setPosition(const glm::vec3& position) {
        if (m_rigidBody) {
            btTransform transform = m_rigidBody->getWorldTransform();
            transform.setOrigin(glmToBullet(position));
            m_rigidBody->setWorldTransform(transform);
            
            if (m_motionState) {
                m_motionState->setWorldTransform(transform);
            }
            
            m_rigidBody->activate();
        }
    }

    void PhysicsComponent::setRotation(const glm::quat& rotation) {
        if (m_rigidBody) {
            btTransform transform = m_rigidBody->getWorldTransform();
            transform.setRotation(glmToBullet(rotation));
            m_rigidBody->setWorldTransform(transform);
            
            if (m_motionState) {
                m_motionState->setWorldTransform(transform);
            }
            
            m_rigidBody->activate();
        }
    }

    glm::vec3 PhysicsComponent::getPosition() const {
        if (m_rigidBody) {
            const btTransform& transform = m_rigidBody->getWorldTransform();
            return bulletToGlm(transform.getOrigin());
        }
        return glm::vec3(0.0f);
    }

    glm::quat PhysicsComponent::getRotation() const {
        if (m_rigidBody) {
            const btTransform& transform = m_rigidBody->getWorldTransform();
            return bulletToGlm(transform.getRotation());
        }
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    // Material properties
    void PhysicsComponent::setFriction(float friction) {
        if (m_rigidBody) {
            m_rigidBody->setFriction(friction);
        }
    }

    void PhysicsComponent::setRestitution(float restitution) {
        if (m_rigidBody) {
            m_rigidBody->setRestitution(restitution);
        }
    }

    void PhysicsComponent::setLinearDamping(float damping) {
        if (m_rigidBody) {
            m_rigidBody->setDamping(damping, m_rigidBody->getAngularDamping());
        }
    }

    void PhysicsComponent::setAngularDamping(float damping) {
        if (m_rigidBody) {
            m_rigidBody->setDamping(m_rigidBody->getLinearDamping(), damping);
        }
    }

    float PhysicsComponent::getFriction() const {
        return m_rigidBody ? m_rigidBody->getFriction() : 0.5f;
    }

    float PhysicsComponent::getRestitution() const {
        return m_rigidBody ? m_rigidBody->getRestitution() : 0.3f;
    }

    float PhysicsComponent::getLinearDamping() const {
        return m_rigidBody ? m_rigidBody->getLinearDamping() : 0.1f;
    }

    float PhysicsComponent::getAngularDamping() const {
        return m_rigidBody ? m_rigidBody->getAngularDamping() : 0.1f;
    }

    // Body state control
    void PhysicsComponent::setKinematic(bool kinematic) {
        if (m_rigidBody) {
            if (kinematic) {
                m_rigidBody->setCollisionFlags(m_rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
                m_rigidBody->setActivationState(DISABLE_DEACTIVATION);
            } else {
                m_rigidBody->setCollisionFlags(m_rigidBody->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
                m_rigidBody->setActivationState(WANTS_DEACTIVATION);
            }
        }
    }

    void PhysicsComponent::setActive(bool active) {
        if (m_rigidBody) {
            if (active) {
                m_rigidBody->activate();
            } else {
                m_rigidBody->setActivationState(WANTS_DEACTIVATION);
            }
        }
    }

    void PhysicsComponent::setGravity(const glm::vec3& gravity) {
        if (m_rigidBody) {
            m_rigidBody->setGravity(glmToBullet(gravity));
        }
    }

    bool PhysicsComponent::isKinematic() const {
        return m_rigidBody && (m_rigidBody->getCollisionFlags() & btCollisionObject::CF_KINEMATIC_OBJECT);
    }

    bool PhysicsComponent::isActive() const {
        return m_rigidBody && m_rigidBody->isActive();
    }

    bool PhysicsComponent::isStatic() const {
        return m_bodyType == BodyType::STATIC;
    }

    bool PhysicsComponent::isDynamic() const {
        return m_bodyType == BodyType::DYNAMIC;
    }

    glm::vec3 PhysicsComponent::getGravity() const {
        if (m_rigidBody) {
            return bulletToGlm(m_rigidBody->getGravity());
        }
        return glm::vec3(0.0f, -9.81f, 0.0f);
    }

    // Collision detection
    void PhysicsComponent::setCollisionGroup(int group) {
        m_collisionGroup = group;
        // Note: The physics system needs to handle updating the world's collision filtering
    }

    void PhysicsComponent::setCollisionMask(int mask) {
        m_collisionMask = mask;
        // Note: The physics system needs to handle updating the world's collision filtering
    }

    int PhysicsComponent::getCollisionGroup() const {
        return m_collisionGroup;
    }

    int PhysicsComponent::getCollisionMask() const {
        return m_collisionMask;
    }

    // Advanced features
    void PhysicsComponent::setCollisionCallbackEnabled(bool enabled) {
        if (m_rigidBody) {
            if (enabled) {
                m_rigidBody->setCollisionFlags(m_rigidBody->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
            } else {
                m_rigidBody->setCollisionFlags(m_rigidBody->getCollisionFlags() & ~btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
            }
        }
    }

    void PhysicsComponent::setContinuousCollisionDetection(float threshold) {
        if (m_rigidBody) {
            m_rigidBody->setCcdMotionThreshold(threshold);
            if (threshold > 0.0f) {
                // Set CCD swept sphere radius to 60% of the smallest dimension
                float radius = std::min({m_dimensions.x, m_dimensions.y, m_dimensions.z}) * 0.6f;
                m_rigidBody->setCcdSweptSphereRadius(radius);
            }
        }
    }

    void PhysicsComponent::setDeactivationThreshold(float linear, float angular) {
        if (m_rigidBody) {
            m_rigidBody->setSleepingThresholds(linear, angular);
        }
    }

    // Transform synchronization
    void PhysicsComponent::updateTransform() {
        if (!m_rigidBody) return;
        
        auto entity = getEntity().lock();
        if (!entity) return;
        
        auto transform = entity->getComponent<TransformComponent>();
        if (!transform) return;

        // Update entity transform from physics simulation
        const btTransform& worldTransform = m_rigidBody->getWorldTransform();
        
        transform->position = bulletToGlm(worldTransform.getOrigin());
        
        // Convert quaternion to Euler angles
        glm::quat rotation = bulletToGlm(worldTransform.getRotation());
        transform->rotation = glm::degrees(glm::eulerAngles(rotation));
    }

    void PhysicsComponent::syncFromTransform() {
        if (!m_rigidBody) return;
        
        auto entity = getEntity().lock();
        if (!entity) return;
        
        auto transform = entity->getComponent<TransformComponent>();
        if (!transform) return;

        // Update physics body from entity transform
        btTransform worldTransform;
        worldTransform.setOrigin(glmToBullet(transform->position));
        
        // Convert Euler angles to quaternion
        glm::quat rotation = glm::quat(glm::radians(transform->rotation));
        worldTransform.setRotation(glmToBullet(rotation));
        
        m_rigidBody->setWorldTransform(worldTransform);
        if (m_motionState) {
            m_motionState->setWorldTransform(worldTransform);
        }
    }

    // Factory methods
    std::shared_ptr<PhysicsComponent> PhysicsComponent::createStaticBox(const glm::vec3& size) {
        auto component = std::make_shared<PhysicsComponent>();
        component->initializeRigidBody(BodyType::STATIC, ColliderType::BOX, size, 0.0f);
        return component;
    }

    std::shared_ptr<PhysicsComponent> PhysicsComponent::createDynamicBox(const glm::vec3& size, float mass) {
        auto component = std::make_shared<PhysicsComponent>();
        component->initializeRigidBody(BodyType::DYNAMIC, ColliderType::BOX, size, mass);
        return component;
    }

    std::shared_ptr<PhysicsComponent> PhysicsComponent::createStaticSphere(float radius) {
        auto component = std::make_shared<PhysicsComponent>();
        component->initializeRigidBody(BodyType::STATIC, ColliderType::SPHERE, glm::vec3(radius), 0.0f);
        return component;
    }

    std::shared_ptr<PhysicsComponent> PhysicsComponent::createDynamicSphere(float radius, float mass) {
        auto component = std::make_shared<PhysicsComponent>();
        component->initializeRigidBody(BodyType::DYNAMIC, ColliderType::SPHERE, glm::vec3(radius), mass);
        return component;
    }

    std::shared_ptr<PhysicsComponent> PhysicsComponent::createStaticPlane(const glm::vec3& normal) {
        auto component = std::make_shared<PhysicsComponent>();
        component->initializeRigidBody(BodyType::STATIC, ColliderType::PLANE, normal, 0.0f);
        return component;
    }

    std::shared_ptr<PhysicsComponent> PhysicsComponent::createCharacterCapsule(float radius, float height, float mass) {
        auto component = std::make_shared<PhysicsComponent>();
        component->initializeRigidBody(BodyType::DYNAMIC, ColliderType::CAPSULE, glm::vec3(radius, height, 0.0f), mass);
        return component;
    }

    // Helper conversion methods
    btVector3 PhysicsComponent::glmToBullet(const glm::vec3& vec) const {
        return btVector3(vec.x, vec.y, vec.z);
    }

    glm::vec3 PhysicsComponent::bulletToGlm(const btVector3& vec) const {
        return glm::vec3(vec.getX(), vec.getY(), vec.getZ());
    }

    btQuaternion PhysicsComponent::glmToBullet(const glm::quat& quat) const {
        return btQuaternion(quat.x, quat.y, quat.z, quat.w);
    }

    glm::quat PhysicsComponent::bulletToGlm(const btQuaternion& quat) const {
        return glm::quat(quat.getW(), quat.getX(), quat.getY(), quat.getZ());
    }

} // namespace IKore
