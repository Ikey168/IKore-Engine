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
#include "components/MaterialComponent.h"

namespace IKore {

    // Type aliases for convenience
    using Transform = TransformComponent;
    using Material = MaterialComponent;
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
     * // Configure material component
     * auto material = entity->addComponent<MaterialComponent>();
     * material->setDiffuseTexture("assets/textures/diffuse.png");
     * material->setNormalTexture("assets/textures/normal.png");
     * material->setSpecularTexture("assets/textures/specular.png");
     * material->loadShader("shaders/vertex.glsl", "shaders/fragment.glsl");
     * material->setDiffuseColor(glm::vec3(0.8f, 0.6f, 0.4f));
     * material->setShininess(64.0f);
     * material->setShaderParameter("metallic", 0.5f);
     * 
     * // Or create material presets
     * auto coloredMaterial = MaterialComponent::createColoredMaterial(
     *     glm::vec3(1.0f, 0.0f, 0.0f), // red diffuse
     *     glm::vec3(0.5f), // specular
     *     32.0f // shininess
     * );
     * entity->addComponent<MaterialComponent>(*coloredMaterial);
     * 
     * // Or create custom mesh data
     * std::vector<Vertex> vertices = {}; // vertex data
     * std::vector<unsigned int> indices = {}; // index data
     * Material meshMaterial{}; // Set up material properties
     * mesh->setMeshData(vertices, indices, meshMaterial);
     * 
     * // Configure velocity
     * velocity->velocity = glm::vec3(5.0f, 0.0f, 0.0f);
     * velocity->maxSpeed = 10.0f;
     * 
     * // Query components
     * if (entity->hasComponent<MeshComponent>()) {
     *     auto meshComp = entity->getComponent<MeshComponent>();
     *     if (meshComp->hasMeshes()) {
     *         // Apply material before rendering
     *         if (entity->hasComponent<MaterialComponent>()) {
     *             auto matComp = entity->getComponent<MaterialComponent>();
     *             matComp->apply();
     *         }
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
     *    - MaterialComponent: Material properties, textures, and shader parameters
     *    - VelocityComponent: Only movement data (velocity, acceleration)
     * 
     * 2. **Entity Composition**: Combine components to create different entity types
     *    - Static File-based Object: Transform + Renderable
     *    - Static Procedural Object: Transform + MeshComponent + MaterialComponent
     *    - Advanced Rendered Object: Transform + MeshComponent + MaterialComponent
     *    - Moving File-based Object: Transform + Renderable + Velocity
     *    - Moving Procedural Object: Transform + MeshComponent + MaterialComponent + Velocity
     *    - Invisible Trigger: Transform + Velocity (no rendering components)
     * 
     * 3. **Rendering Component Choice**:
     *    - Use RenderableComponent for models loaded from files (efficient for static assets)
     *    - Use MeshComponent for procedural geometry, runtime-generated meshes, or dynamic modifications
     *    - Use MaterialComponent for advanced material control (PBR, custom shaders, dynamic properties)
     *    - MeshComponent + MaterialComponent provides maximum rendering control
     * 
     * 4. **Performance**: Cache component pointers when accessing frequently
     *    - Store component references in systems that update them every frame
     *    - Use hasComponent<T>() before getComponent<T>() for safety
     *    - For MaterialComponent, cache shader references and avoid redundant texture bindings
     *    - For MeshComponent, consider caching mesh pointers for repeated rendering
     * 
     * 5. **Memory Management**: Components are automatically managed
     *    - Use shared_ptr for components returned by addComponent<T>()
     *    - Components are automatically destroyed when entity is destroyed
     *    - Use weak_ptr in components to reference the owning entity
     *    - MaterialComponent uses texture caching to optimize memory usage
     *    - MeshComponent owns its mesh data and handles cleanup automatically
     */

} // namespace IKore
