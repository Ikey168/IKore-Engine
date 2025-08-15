#pragma once

/**
 * @file Components.h
 * @brief Convenience header for all component types and usage examples
 * 
 * This header provides easy access to all component types in the IKore Engine
 * and includes usage examples for the Entity-Component System.
 */

#include "Component.h"
#include "components/TransformComponent.h"
#include "components/RenderableComponent.h"
#include "components/VelocityComponent.h"

namespace IKore {

    // Type aliases for convenience
    using Transform = TransformComponent;
    using Renderable = RenderableComponent;
    using Velocity = VelocityComponent;

    /**
     * @brief Component System Usage Examples
     * 
     * Here are some examples of how to use the component system:
     * 
     * @code
     * // Create an entity
     * auto entity = std::make_shared<Entity>();
     * 
     * // Add components
     * auto transform = entity->addComponent<TransformComponent>();
     * auto renderable = entity->addComponent<RenderableComponent>();
     * auto velocity = entity->addComponent<VelocityComponent>();
     * 
     * // Configure components
     * transform->position = glm::vec3(0.0f, 0.0f, 0.0f);
     * transform->scale = glm::vec3(1.0f);
     * 
     * renderable->meshPath = "assets/models/player.obj";
     * renderable->texturePath = "assets/textures/player_diffuse.png";
     * renderable->visible = true;
     * 
     * velocity->velocity = glm::vec3(5.0f, 0.0f, 0.0f);
     * velocity->maxSpeed = 10.0f;
     * 
     * // Query components
     * if (entity->hasComponent<VelocityComponent>()) {
     *     auto vel = entity->getComponent<VelocityComponent>();
     *     vel->integrate(deltaTime);
     * }
     * 
     * // Update transform based on velocity
     * if (entity->hasComponent<TransformComponent>() && 
     *     entity->hasComponent<VelocityComponent>()) {
     *     
     *     auto transform = entity->getComponent<TransformComponent>();
     *     auto velocity = entity->getComponent<VelocityComponent>();
     *     
     *     transform->position += velocity->velocity * deltaTime;
     * }
     * 
     * // Remove components when no longer needed
     * entity->removeComponent<VelocityComponent>();
     * @endcode
     */

    /**
     * @brief Component System Best Practices
     * 
     * 1. **Component Design**: Keep components focused on a single responsibility
     *    - TransformComponent: Only spatial data (position, rotation, scale)
     *    - RenderableComponent: Only rendering data (mesh, textures, visibility)
     *    - VelocityComponent: Only movement data (velocity, acceleration)
     * 
     * 2. **Entity Composition**: Combine components to create different entity types
     *    - Static Object: Transform + Renderable
     *    - Moving Object: Transform + Renderable + Velocity
     *    - Invisible Trigger: Transform + Velocity (no Renderable)
     * 
     * 3. **Performance**: Cache component pointers when accessing frequently
     *    - Store component references in systems that update them every frame
     *    - Use hasComponent<T>() before getComponent<T>() for safety
     * 
     * 4. **Memory Management**: Components are automatically managed
     *    - Use shared_ptr for components returned by addComponent<T>()
     *    - Components are automatically destroyed when entity is destroyed
     *    - Use weak_ptr in components to reference the owning entity
     */

} // namespace IKore
