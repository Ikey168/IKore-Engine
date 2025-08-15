#pragma once

/**
 * @file Components.h
 * @brief Convenience header for all component types and usage examples
 * 
 * This header provides easy access to all component types in the IKore Engine
 * and includes usage examples for the Entity-Component System.
 */

#include "Component.h"

namespace IKore {

    /**
     * @brief Component System Usage Examples
     * 
     * Here are some examples of how to use the component system:
     * 
     * @code
     * // Create an entity
     * auto entity = std::make_shared<Entity>();
     * 
     * // Add components (using custom component classes)
     * auto testComp = entity->addComponent<TestComponent>();
     * 
     * // Configure components
     * testComp->value = 100;
     * 
     * // Query components
     * if (entity->hasComponent<TestComponent>()) {
     *     auto comp = entity->getComponent<TestComponent>();
     *     comp->doSomething();
     * }
     * 
     * // Remove components when no longer needed
     * entity->removeComponent<TestComponent>();
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
