#pragma once

#include "../Component.h"
#include <glm/glm.hpp>
#include <btBulletDynamicsCommon.h>
#include <memory>

namespace IKore {

    /**
     * @brief Supported collider types for physics bodies
     */
    enum class ColliderType {
        BOX,
        SPHERE,
        CAPSULE,
        MESH,
        PLANE
    };

    /**
     * @brief Supported body types for physics simulation
     */
    enum class BodyType {
        STATIC,     // Zero mass, immovable (walls, floors)
        KINEMATIC,  // Zero mass, movable by code (moving platforms)
        DYNAMIC     // Has mass, affected by forces (player, objects)
    };

    /**
     * @brief PhysicsComponent - Provides physics simulation capabilities using Bullet Physics
     * 
     * This component integrates Bullet Physics into the ECS system, providing:
     * - RigidBody simulation with mass, velocity, and acceleration
     * - Collision detection with various primitive shapes
     * - Force and impulse application
     * - Integration with the transform system
     * 
     * Usage Examples:
     * 
     * @code
     * // Create a dynamic box collider
     * auto entity = std::make_shared<Entity>();
     * auto physics = entity->addComponent<PhysicsComponent>();
     * physics->initializeRigidBody(BodyType::DYNAMIC, ColliderType::BOX, 
     *                             glm::vec3(1.0f), 10.0f);
     * 
     * // Create a static ground plane
     * auto ground = std::make_shared<Entity>();
     * auto groundPhysics = ground->addComponent<PhysicsComponent>();
     * groundPhysics->initializeRigidBody(BodyType::STATIC, ColliderType::PLANE, 
     *                                   glm::vec3(0.0f, 1.0f, 0.0f), 0.0f);
     * 
     * // Apply forces
     * physics->applyForce(glm::vec3(0.0f, 100.0f, 0.0f)); // Jump
     * physics->applyImpulse(glm::vec3(10.0f, 0.0f, 0.0f)); // Quick push
     * 
     * // Set material properties
     * physics->setFriction(0.8f);
     * physics->setRestitution(0.3f); // Bounciness
     * 
     * // Get physics state
     * glm::vec3 velocity = physics->getLinearVelocity();
     * glm::vec3 position = physics->getPosition();
     * @endcode
     */
    class PhysicsComponent : public Component {
    public:
        PhysicsComponent();
        virtual ~PhysicsComponent();

        // Disable copy constructor and assignment to prevent Bullet object duplication
        PhysicsComponent(const PhysicsComponent&) = delete;
        PhysicsComponent& operator=(const PhysicsComponent&) = delete;

        /**
         * @brief Initialize the rigid body with specified parameters
         * @param bodyType Type of physics body (static, kinematic, dynamic)
         * @param colliderType Shape of the collider
         * @param dimensions Size parameters (box: width/height/depth, sphere: radius, capsule: radius/height)
         * @param mass Mass of the body (0.0 for static/kinematic bodies)
         */
        void initializeRigidBody(BodyType bodyType, ColliderType colliderType, 
                               const glm::vec3& dimensions, float mass = 1.0f);

        /**
         * @brief Initialize with a custom collision shape
         * @param bodyType Type of physics body
         * @param shape Custom Bullet collision shape (takes ownership)
         * @param mass Mass of the body
         */
        void initializeRigidBody(BodyType bodyType, std::unique_ptr<btCollisionShape> shape, float mass = 1.0f);

        // Force and impulse application
        void applyForce(const glm::vec3& force);
        void applyForce(const glm::vec3& force, const glm::vec3& relativePos);
        void applyImpulse(const glm::vec3& impulse);
        void applyImpulse(const glm::vec3& impulse, const glm::vec3& relativePos);
        void applyTorque(const glm::vec3& torque);
        void applyTorqueImpulse(const glm::vec3& torque);

        // Velocity control
        void setLinearVelocity(const glm::vec3& velocity);
        void setAngularVelocity(const glm::vec3& velocity);
        glm::vec3 getLinearVelocity() const;
        glm::vec3 getAngularVelocity() const;

        // Position and rotation (syncs with transform component)
        void setPosition(const glm::vec3& position);
        void setRotation(const glm::quat& rotation);
        glm::vec3 getPosition() const;
        glm::quat getRotation() const;

        // Material properties
        void setFriction(float friction);
        void setRestitution(float restitution);
        void setLinearDamping(float damping);
        void setAngularDamping(float damping);
        
        float getFriction() const;
        float getRestitution() const;
        float getLinearDamping() const;
        float getAngularDamping() const;

        // Body state control
        void setKinematic(bool kinematic);
        void setActive(bool active);
        void setGravity(const glm::vec3& gravity);
        
        bool isKinematic() const;
        bool isActive() const;
        bool isStatic() const;
        bool isDynamic() const;
        glm::vec3 getGravity() const;

        // Collision detection
        void setCollisionGroup(int group);
        void setCollisionMask(int mask);
        int getCollisionGroup() const;
        int getCollisionMask() const;

        // Advanced features
        void setCollisionCallbackEnabled(bool enabled);
        void setContinuousCollisionDetection(float threshold);
        void setDeactivationThreshold(float linear, float angular);

        // Physics world integration (called by physics system)
        void updateTransform(); // Sync physics transform to entity transform
        void syncFromTransform(); // Sync entity transform to physics
        
        // Internal access for physics system
        btRigidBody* getRigidBody() const { return m_rigidBody.get(); }
        bool isInitialized() const { return m_rigidBody != nullptr; }

        // Factory methods for common configurations
        static std::shared_ptr<PhysicsComponent> createStaticBox(const glm::vec3& size);
        static std::shared_ptr<PhysicsComponent> createDynamicBox(const glm::vec3& size, float mass);
        static std::shared_ptr<PhysicsComponent> createStaticSphere(float radius);
        static std::shared_ptr<PhysicsComponent> createDynamicSphere(float radius, float mass);
        static std::shared_ptr<PhysicsComponent> createStaticPlane(const glm::vec3& normal);
        static std::shared_ptr<PhysicsComponent> createCharacterCapsule(float radius, float height, float mass);

    private:
        // Core Bullet Physics objects
        std::unique_ptr<btCollisionShape> m_collisionShape;
        std::unique_ptr<btDefaultMotionState> m_motionState;
        std::unique_ptr<btRigidBody> m_rigidBody;

        // Configuration
        BodyType m_bodyType = BodyType::DYNAMIC;
        ColliderType m_colliderType = ColliderType::BOX;
        glm::vec3 m_dimensions = glm::vec3(1.0f);
        float m_mass = 1.0f;

        // Collision filtering
        int m_collisionGroup = 1;
        int m_collisionMask = -1; // Collide with everything by default

        // Helper methods
        void createCollisionShape(ColliderType type, const glm::vec3& dimensions);
        void setupRigidBody(float mass);
        btVector3 glmToBullet(const glm::vec3& vec) const;
        glm::vec3 bulletToGlm(const btVector3& vec) const;
        btQuaternion glmToBullet(const glm::quat& quat) const;
        glm::quat bulletToGlm(const btQuaternion& quat) const;
    };

} // namespace IKore
