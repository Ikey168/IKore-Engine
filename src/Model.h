#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Texture.h"
#include "Frustum.h"

namespace IKore {

// Forward declaration
class Shader;

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

struct Material {
    std::shared_ptr<Texture> diffuse;
    std::shared_ptr<Texture> specular;
    std::shared_ptr<Texture> normal;
    std::shared_ptr<Texture> height;
    std::shared_ptr<Texture> emissive;
    
    // Fallback color values
    glm::vec3 diffuseColor = glm::vec3(1.0f);
    glm::vec3 specularColor = glm::vec3(0.5f);
    glm::vec3 ambientColor = glm::vec3(0.2f);
    float shininess = 32.0f;
    
    // Flags to indicate texture availability
    bool hasDiffuseTexture() const { return diffuse && diffuse->isLoaded(); }
    bool hasSpecularTexture() const { return specular && specular->isLoaded(); }
    bool hasNormalTexture() const { return normal && normal->isLoaded(); }
    bool hasHeightTexture() const { return height && height->isLoaded(); }
    bool hasEmissiveTexture() const { return emissive && emissive->isLoaded(); }
};

class Mesh {
private:
    std::vector<Vertex> m_vertices;
    std::vector<unsigned int> m_indices;
    Material m_material;
    BoundingBox m_boundingBox;
    
    GLuint m_VAO, m_VBO, m_EBO;
    bool m_isSetup;
    
    void setupMesh();
    void calculateBoundingBox();

public:
    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, Material material);
    ~Mesh();
    
    // Delete copy constructor and assignment operator
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    
    // Move constructor and assignment operator
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    
    void render(std::shared_ptr<Shader> shader) const;
    
    // Getters
    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<unsigned int>& getIndices() const { return m_indices; }
    const Material& getMaterial() const { return m_material; }
    const BoundingBox& getBoundingBox() const { return m_boundingBox; }
    bool isSetup() const { return m_isSetup; }
};

class Model {
private:
    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::string m_directory;
    std::string m_path;
    BoundingBox m_boundingBox;
    
    // Texture cache to avoid loading duplicate textures
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textureCache;
    
    // Assimp processing
    void loadModel(const std::string& path);
    void processNode(aiNode* node, const aiScene* scene);
    std::unique_ptr<Mesh> processMesh(aiMesh* mesh, const aiScene* scene);
    Material loadMaterial(aiMaterial* mat);
    std::shared_ptr<Texture> loadMaterialTexture(aiMaterial* mat, aiTextureType type, 
                                                Texture::Type textureType);
    
    // Helper functions
    std::string getTextureFilename(aiMaterial* mat, aiTextureType type, unsigned int index = 0);
    Texture::Type aiTextureTypeToTextureType(aiTextureType type);
    void calculateModelBoundingBox();

public:
    Model() = default;
    ~Model() = default;
    
    // Delete copy constructor and assignment operator
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    
    // Move constructor and assignment operator
    Model(Model&& other) noexcept;
    Model& operator=(Model&& other) noexcept;
    
    // Load model from file
    bool loadFromFile(const std::string& path);
    
    // Render the entire model
    void render(std::shared_ptr<Shader> shader) const;
    
    // Getters
    const std::vector<std::unique_ptr<Mesh>>& getMeshes() const { return m_meshes; }
    const std::string& getPath() const { return m_path; }
    const std::string& getDirectory() const { return m_directory; }
    const BoundingBox& getBoundingBox() const { return m_boundingBox; }
    size_t getMeshCount() const { return m_meshes.size(); }
    bool isEmpty() const { return m_meshes.empty(); }
    
    // Static utility functions
    static std::shared_ptr<Model> loadFromFileShared(const std::string& path);
    static std::string getSupportedFormats();
};

} // namespace IKore
