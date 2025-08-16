#include "MeshComponent.h"
#include "../../Shader.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

namespace IKore {

void MeshComponent::setMeshData(const std::vector<Vertex>& vertices, 
                               const std::vector<unsigned int>& indices,
                               const Material& material) {
    // Clear existing meshes
    clearMeshes();
    
    // Add the new mesh
    addMesh(vertices, indices, material);
}

void MeshComponent::setMeshData(const Mesh& mesh) {
    // Clear existing meshes
    clearMeshes();
    
    // Create a new mesh from the existing one
    meshes_.emplace_back(std::make_unique<Mesh>(
        mesh.getVertices(),
        mesh.getIndices(),
        mesh.getMaterial()
    ));
}

size_t MeshComponent::addMesh(const std::vector<Vertex>& vertices,
                             const std::vector<unsigned int>& indices,
                             const Material& material) {
    // Create a new mesh and add it to our collection
    meshes_.emplace_back(std::make_unique<Mesh>(vertices, indices, material));
    
    // Return the index of the newly added mesh
    return meshes_.size() - 1;
}

void MeshComponent::clearMeshes() {
    meshes_.clear();
}

const Mesh* MeshComponent::getPrimaryMesh() const {
    return meshes_.empty() ? nullptr : meshes_[0].get();
}

const Mesh* MeshComponent::getMesh(size_t index) const {
    if (index >= meshes_.size()) {
        return nullptr;
    }
    return meshes_[index].get();
}

void MeshComponent::render(std::shared_ptr<Shader> shader) const {
    if (!visible || !shader) {
        return;
    }

    // Render all meshes
    for (const auto& mesh : meshes_) {
        if (mesh) {
            mesh->render(shader);
        }
    }
}

BoundingBox MeshComponent::getBoundingBox() const {
    if (meshes_.empty()) {
        return BoundingBox{};
    }

    BoundingBox combinedBounds = meshes_[0]->getBoundingBox();
    
    // Combine bounding boxes from all meshes
    for (size_t i = 1; i < meshes_.size(); ++i) {
        const BoundingBox& meshBounds = meshes_[i]->getBoundingBox();
        
        // Expand the combined bounds
        combinedBounds.min.x = std::min(combinedBounds.min.x, meshBounds.min.x);
        combinedBounds.min.y = std::min(combinedBounds.min.y, meshBounds.min.y);
        combinedBounds.min.z = std::min(combinedBounds.min.z, meshBounds.min.z);
        
        combinedBounds.max.x = std::max(combinedBounds.max.x, meshBounds.max.x);
        combinedBounds.max.y = std::max(combinedBounds.max.y, meshBounds.max.y);
        combinedBounds.max.z = std::max(combinedBounds.max.z, meshBounds.max.z);
    }
    
    return combinedBounds;
}

size_t MeshComponent::getTotalVertexCount() const {
    size_t totalVertices = 0;
    for (const auto& mesh : meshes_) {
        totalVertices += mesh->getVertices().size();
    }
    return totalVertices;
}

size_t MeshComponent::getTotalIndexCount() const {
    size_t totalIndices = 0;
    for (const auto& mesh : meshes_) {
        totalIndices += mesh->getIndices().size();
    }
    return totalIndices;
}

// === Static Factory Methods ===

std::shared_ptr<MeshComponent> MeshComponent::createCube(float size, const Material& material) {
    auto component = std::make_shared<MeshComponent>();
    
    auto vertices = createCubeVertices(size);
    auto indices = createCubeIndices();
    
    component->setMeshData(vertices, indices, material);
    
    return component;
}

std::shared_ptr<MeshComponent> MeshComponent::createPlane(float width, float height, const Material& material) {
    auto component = std::make_shared<MeshComponent>();
    
    auto vertices = createPlaneVertices(width, height);
    auto indices = createPlaneIndices();
    
    component->setMeshData(vertices, indices, material);
    
    return component;
}

std::shared_ptr<MeshComponent> MeshComponent::createSphere(float radius, int segments, const Material& material) {
    auto component = std::make_shared<MeshComponent>();
    
    auto vertices = createSphereVertices(radius, segments);
    auto indices = createSphereIndices(segments);
    
    component->setMeshData(vertices, indices, material);
    
    return component;
}

// === Component Lifecycle ===

void MeshComponent::onAttach() {
    // Called when component is attached to an entity
    // Could be used for initialization if needed
}

void MeshComponent::onDetach() {
    // Called when component is detached from an entity
    // Cleanup is handled automatically by destructors
}

// === Geometry Generation Helpers ===

std::vector<Vertex> MeshComponent::createCubeVertices(float size) {
    float halfSize = size * 0.5f;
    
    return {
        // Front face
        {{-halfSize, -halfSize,  halfSize}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ halfSize, -halfSize,  halfSize}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ halfSize,  halfSize,  halfSize}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-halfSize,  halfSize,  halfSize}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        
        // Back face
        {{ halfSize, -halfSize, -halfSize}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-halfSize, -halfSize, -halfSize}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-halfSize,  halfSize, -halfSize}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ halfSize,  halfSize, -halfSize}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        
        // Left face
        {{-halfSize, -halfSize, -halfSize}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-halfSize, -halfSize,  halfSize}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-halfSize,  halfSize,  halfSize}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-halfSize,  halfSize, -halfSize}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        
        // Right face
        {{ halfSize, -halfSize,  halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
        {{ halfSize, -halfSize, -halfSize}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
        {{ halfSize,  halfSize, -halfSize}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
        {{ halfSize,  halfSize,  halfSize}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
        
        // Top face
        {{-halfSize,  halfSize,  halfSize}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{ halfSize,  halfSize,  halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{ halfSize,  halfSize, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{-halfSize,  halfSize, -halfSize}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        
        // Bottom face
        {{-halfSize, -halfSize, -halfSize}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ halfSize, -halfSize, -halfSize}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ halfSize, -halfSize,  halfSize}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-halfSize, -halfSize,  halfSize}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}
    };
}

std::vector<unsigned int> MeshComponent::createCubeIndices() {
    return {
        // Front face
        0, 1, 2,   2, 3, 0,
        // Back face
        4, 5, 6,   6, 7, 4,
        // Left face
        8, 9, 10,  10, 11, 8,
        // Right face
        12, 13, 14, 14, 15, 12,
        // Top face
        16, 17, 18, 18, 19, 16,
        // Bottom face
        20, 21, 22, 22, 23, 20
    };
}

std::vector<Vertex> MeshComponent::createPlaneVertices(float width, float height) {
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    
    return {
        {{-halfWidth, 0.0f, -halfHeight}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{ halfWidth, 0.0f, -halfHeight}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{ halfWidth, 0.0f,  halfHeight}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{-halfWidth, 0.0f,  halfHeight}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}}
    };
}

std::vector<unsigned int> MeshComponent::createPlaneIndices() {
    return {
        0, 1, 2,
        2, 3, 0
    };
}

std::vector<Vertex> MeshComponent::createSphereVertices(float radius, int segments) {
    std::vector<Vertex> vertices;
    
    const float PI = glm::pi<float>();
    
    // Generate vertices
    for (int i = 0; i <= segments; ++i) {
        float phi = PI * static_cast<float>(i) / static_cast<float>(segments);
        
        for (int j = 0; j <= segments; ++j) {
            float theta = 2.0f * PI * static_cast<float>(j) / static_cast<float>(segments);
            
            // Spherical coordinates to Cartesian
            float x = radius * std::sin(phi) * std::cos(theta);
            float y = radius * std::cos(phi);
            float z = radius * std::sin(phi) * std::sin(theta);
            
            glm::vec3 position(x, y, z);
            glm::vec3 normal = glm::normalize(position);
            
            // Texture coordinates
            float u = static_cast<float>(j) / static_cast<float>(segments);
            float v = static_cast<float>(i) / static_cast<float>(segments);
            
            // Calculate tangent and bitangent
            glm::vec3 tangent(-std::sin(theta), 0.0f, std::cos(theta));
            glm::vec3 bitangent = glm::cross(normal, tangent);
            
            vertices.push_back({position, normal, glm::vec2(u, v), tangent, bitangent});
        }
    }
    
    return vertices;
}

std::vector<unsigned int> MeshComponent::createSphereIndices(int segments) {
    std::vector<unsigned int> indices;
    
    // Generate indices for triangles
    for (int i = 0; i < segments; ++i) {
        for (int j = 0; j < segments; ++j) {
            int first = (i * (segments + 1)) + j;
            int second = first + segments + 1;
            
            // First triangle
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            
            // Second triangle
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
    
    return indices;
}

} // namespace IKore
