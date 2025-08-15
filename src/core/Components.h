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
#include "components/MeshComponent.h"

namespace IKore {

    // Type aliases for convenience
    using Transform = TransformComponent;
    using Renderable = RenderableComponent;
    using Velocity = VelocityComponent;
    using MeshComp = MeshComponent;  // Avoid conflict with IKore::Mesh class

    /**
     * @brief Component System Usage Examples
     * 
     * Here are some examples of how to use the component system:
     * 
     * @code
     * // Create an entity
     * auto entity = std::make_shared<Entity>();
     * 
     * // Add built-in components
     * auto transform = entity->addComponent<TransformComponent>();
     * auto renderable = entity->addComponent<RenderableComponent>();
     * auto mesh = entity->addComponent<MeshComponent>();
     * auto velocity = entity->addComponent<VelocityComponent>();
     * 
     * // Configure transform
     * transform->position = glm::vec3(0.0f, 0.0f, 0.0f);
     * transform->rotation = glm::vec3(0.0f, 45.0f, 0.0f);
     * transform->scale = glm::vec3(1.0f, 1.0f, 1.0f);
     * 
     * // Configure renderable (for file-based meshes)
     * renderable->visible = true;
     * renderable->meshPath = "assets/models/player.obj";
     * renderable->texturePath = "assets/textures/player_diffuse.png";
     * 
     * // Configure mesh component (for procedural/runtime meshes)
     * auto cubeMesh = MeshComponent::createCube(2.0f);
     * entity->addComponent<MeshComponent>()->setMeshData(*cubeMesh->getPrimaryMesh());
     * 
     * // Or create custom mesh data
     * std::vector<Vertex> vertices = {}; // vertex data
     * std::vector<unsigned int> indices = {}; // index data
     * Material material{}; // Set up material properties
     * mesh->setMeshData(vertices, indices, material);
     * 
     * // Configure velocity
     * velocity->velocity = glm::vec3(5.0f, 0.0f, 0.0f);
     * velocity->maxSpeed = 10.0f;
     * 
     * // Query components
     * if (entity->hasComponent<MeshComponent>()) {
     *     auto meshComp = entity->getComponent<MeshComponent>();
     *     if (meshComp->hasMeshes()) {
     *         // Render the mesh
     *         meshComp->render(shader);
     *     }
     * }
     * 
     * // Remove components when no longer needed
     * entity->removeComponent<VelocityComponent>();
     */

    /**
     * @brief Component System Best Practices
     * 
     * 1. **Component Design**: Keep components focused on a single responsibility
     *    - TransformComponent: Only spatial data (position, rotation, scale)
     *    - RenderableComponent: File-based rendering data (mesh paths, texture paths)
     *    - MeshComponent: Direct mesh data storage (vertices, indices, materials)
     *    - VelocityComponent: Only movement data (velocity, acceleration)
     * 
     * 2. **Entity Composition**: Combine components to create different entity types
     *    - Static File-based Object: Transform + Renderable
     *    - Static Procedural Object: Transform + MeshComponent
     *    - Moving File-based Object: Transform + Renderable + Velocity
     *    - Moving Procedural Object: Transform + MeshComponent + Velocity
     *    - Invisible Trigger: Transform + Velocity (no rendering components)
     * 
     * 3. **Rendering Component Choice**:
     *    - Use RenderableComponent for models loaded from files (efficient for static assets)
     *    - Use MeshComponent for procedural geometry, runtime-generated meshes, or dynamic modifications
     *    - MeshComponent provides direct mesh data access for advanced use cases
     * 
     * 4. **Performance**: Cache component pointers when accessing frequently
     *    - Store component references in systems that update them every frame
     *    - Use hasComponent<T>() before getComponent<T>() for safety
     *    - For MeshComponent, consider caching mesh pointers for repeated rendering
     * 
     * 5. **Memory Management**: Components are automatically managed
     *    - Use shared_ptr for components returned by addComponent<T>()
     *    - Components are automatically destroyed when entity is destroyed
     *    - Use weak_ptr in components to reference the owning entity
     *    - MeshComponent owns its mesh data and handles cleanup automatically
     */

} // namespace IKore
