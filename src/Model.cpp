#include "Model.h"
#include "Shader.h"
#include "Logger.h"

#include <iostream>
#include <filesystem>

namespace IKore {

// Mesh Implementation
Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, Material material)
    : m_vertices(std::move(vertices)), m_indices(std::move(indices)), m_material(std::move(material)),
      m_VAO(0), m_VBO(0), m_EBO(0), m_isSetup(false) {
    setupMesh();
}

Mesh::~Mesh() {
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
}

Mesh::Mesh(Mesh&& other) noexcept 
    : m_vertices(std::move(other.m_vertices)), m_indices(std::move(other.m_indices)),
      m_material(std::move(other.m_material)), m_VAO(other.m_VAO), m_VBO(other.m_VBO),
      m_EBO(other.m_EBO), m_isSetup(other.m_isSetup) {
    other.m_VAO = other.m_VBO = other.m_EBO = 0;
    other.m_isSetup = false;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        // Clean up current resources
        if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
        if (m_VBO) glDeleteBuffers(1, &m_VBO);
        if (m_EBO) glDeleteBuffers(1, &m_EBO);
        
        // Move data
        m_vertices = std::move(other.m_vertices);
        m_indices = std::move(other.m_indices);
        m_material = std::move(other.m_material);
        m_VAO = other.m_VAO;
        m_VBO = other.m_VBO;
        m_EBO = other.m_EBO;
        m_isSetup = other.m_isSetup;
        
        // Reset other object
        other.m_VAO = other.m_VBO = other.m_EBO = 0;
        other.m_isSetup = false;
    }
    return *this;
}

void Mesh::setupMesh() {
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);
    
    glBindVertexArray(m_VAO);
    
    // Load vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), &m_vertices[0], GL_STATIC_DRAW);
    
    // Load index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), &m_indices[0], GL_STATIC_DRAW);
    
    // Vertex positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    
    // Vertex normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    
    // Vertex texture coordinates
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    
    // Vertex tangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
    
    // Vertex bitangent
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitangent));
    
    glBindVertexArray(0);
    m_isSetup = true;
}

void Mesh::render(std::shared_ptr<Shader> shader) const {
    if (!m_isSetup || !shader) return;
    
    shader->use();
    
    // Bind textures and set uniforms
    GLuint textureUnit = 0;
    
    // Diffuse texture
    if (m_material.hasDiffuseTexture()) {
        m_material.diffuse->bind(textureUnit);
        shader->setFloat("material.useDiffuseTexture", 1.0f);
        glUniform1i(glGetUniformLocation(shader->id(), "material.diffuse"), textureUnit++);
    } else {
        shader->setFloat("material.useDiffuseTexture", 0.0f);
    }
    
    // Specular texture
    if (m_material.hasSpecularTexture()) {
        m_material.specular->bind(textureUnit);
        shader->setFloat("material.useSpecularTexture", 1.0f);
        glUniform1i(glGetUniformLocation(shader->id(), "material.specular"), textureUnit++);
    } else {
        shader->setFloat("material.useSpecularTexture", 0.0f);
    }
    
    // Normal texture
    if (m_material.hasNormalTexture()) {
        m_material.normal->bind(textureUnit);
        shader->setFloat("material.useNormalTexture", 1.0f);
        glUniform1i(glGetUniformLocation(shader->id(), "material.normal"), textureUnit++);
    } else {
        shader->setFloat("material.useNormalTexture", 0.0f);
    }
    
    // Set material color properties
    shader->setVec3("material.ambientColor", m_material.ambientColor.x, m_material.ambientColor.y, m_material.ambientColor.z);
    shader->setVec3("material.diffuseColor", m_material.diffuseColor.x, m_material.diffuseColor.y, m_material.diffuseColor.z);
    shader->setVec3("material.specularColor", m_material.specularColor.x, m_material.specularColor.y, m_material.specularColor.z);
    shader->setFloat("material.shininess", m_material.shininess);
    
    // Draw mesh
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(m_indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Unbind textures
    glActiveTexture(GL_TEXTURE0);
}

// Model Implementation
Model::Model(Model&& other) noexcept 
    : m_meshes(std::move(other.m_meshes)), m_directory(std::move(other.m_directory)),
      m_path(std::move(other.m_path)), m_textureCache(std::move(other.m_textureCache)) {
}

Model& Model::operator=(Model&& other) noexcept {
    if (this != &other) {
        m_meshes = std::move(other.m_meshes);
        m_directory = std::move(other.m_directory);
        m_path = std::move(other.m_path);
        m_textureCache = std::move(other.m_textureCache);
    }
    return *this;
}

bool Model::loadFromFile(const std::string& path) {
    LOG_INFO("Loading model from: " + path);
    
    if (!std::filesystem::exists(path)) {
        LOG_ERROR("Model file does not exist: " + path);
        return false;
    }
    
    m_path = path;
    m_directory = path.substr(0, path.find_last_of('/'));
    
    try {
        loadModel(path);
        LOG_INFO("Successfully loaded model: " + path + " with " + std::to_string(m_meshes.size()) + " meshes");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load model: " + path + " - " + std::string(e.what()));
        return false;
    }
}

void Model::loadModel(const std::string& path) {
    Assimp::Importer importer;
    
    // Configure Assimp post-processing flags
    unsigned int flags = aiProcess_Triangulate |
                        aiProcess_FlipUVs |
                        aiProcess_CalcTangentSpace |
                        aiProcess_GenSmoothNormals |
                        aiProcess_JoinIdenticalVertices |
                        aiProcess_ImproveCacheLocality |
                        aiProcess_OptimizeMeshes |
                        aiProcess_OptimizeGraph;
    
    const aiScene* scene = importer.ReadFile(path, flags);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw std::runtime_error("Assimp error: " + std::string(importer.GetErrorString()));
    }
    
    processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene) {
    // Process all meshes in the current node
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        m_meshes.push_back(processMesh(mesh, scene));
    }
    
    // Process all child nodes
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

std::unique_ptr<Mesh> Model::processMesh(aiMesh* mesh, const aiScene* scene) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    // Process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        
        // Positions
        vertex.position.x = mesh->mVertices[i].x;
        vertex.position.y = mesh->mVertices[i].y;
        vertex.position.z = mesh->mVertices[i].z;
        
        // Normals
        if (mesh->HasNormals()) {
            vertex.normal.x = mesh->mNormals[i].x;
            vertex.normal.y = mesh->mNormals[i].y;
            vertex.normal.z = mesh->mNormals[i].z;
        } else {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        
        // Texture coordinates
        if (mesh->mTextureCoords[0]) {
            vertex.texCoords.x = mesh->mTextureCoords[0][i].x;
            vertex.texCoords.y = mesh->mTextureCoords[0][i].y;
        } else {
            vertex.texCoords = glm::vec2(0.0f, 0.0f);
        }
        
        // Tangents
        if (mesh->HasTangentsAndBitangents()) {
            vertex.tangent.x = mesh->mTangents[i].x;
            vertex.tangent.y = mesh->mTangents[i].y;
            vertex.tangent.z = mesh->mTangents[i].z;
            
            vertex.bitangent.x = mesh->mBitangents[i].x;
            vertex.bitangent.y = mesh->mBitangents[i].y;
            vertex.bitangent.z = mesh->mBitangents[i].z;
        } else {
            vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            vertex.bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        
        vertices.push_back(vertex);
    }
    
    // Process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    
    // Process material
    Material material;
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
        material = loadMaterial(mat);
    }
    
    return std::make_unique<Mesh>(std::move(vertices), std::move(indices), std::move(material));
}

Material Model::loadMaterial(aiMaterial* mat) {
    Material material;
    
    // Load textures
    material.diffuse = loadMaterialTexture(mat, aiTextureType_DIFFUSE, Texture::Type::DIFFUSE);
    material.specular = loadMaterialTexture(mat, aiTextureType_SPECULAR, Texture::Type::SPECULAR);
    material.normal = loadMaterialTexture(mat, aiTextureType_HEIGHT, Texture::Type::NORMAL);
    material.height = loadMaterialTexture(mat, aiTextureType_DISPLACEMENT, Texture::Type::HEIGHT);
    material.emissive = loadMaterialTexture(mat, aiTextureType_EMISSIVE, Texture::Type::EMISSIVE);
    
    // Load material properties
    aiColor3D color;
    
    // Diffuse color
    if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
        material.diffuseColor = glm::vec3(color.r, color.g, color.b);
    }
    
    // Specular color
    if (mat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
        material.specularColor = glm::vec3(color.r, color.g, color.b);
    }
    
    // Ambient color
    if (mat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
        material.ambientColor = glm::vec3(color.r, color.g, color.b);
    }
    
    // Shininess
    float shininess;
    if (mat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
        material.shininess = shininess;
    }
    
    return material;
}

std::shared_ptr<Texture> Model::loadMaterialTexture(aiMaterial* mat, aiTextureType type, Texture::Type textureType) {
    if (mat->GetTextureCount(type) == 0) {
        return nullptr;
    }
    
    std::string filename = getTextureFilename(mat, type);
    if (filename.empty()) {
        return nullptr;
    }
    
    // Check cache first
    auto it = m_textureCache.find(filename);
    if (it != m_textureCache.end()) {
        return it->second;
    }
    
    // Load new texture
    std::string fullPath = m_directory + "/" + filename;
    auto texture = Texture::loadFromFileShared(fullPath, textureType);
    
    if (texture) {
        m_textureCache[filename] = texture;
        LOG_INFO("Loaded material texture: " + fullPath);
    } else {
        LOG_WARNING("Failed to load material texture: " + fullPath);
    }
    
    return texture;
}

std::string Model::getTextureFilename(aiMaterial* mat, aiTextureType type, unsigned int index) {
    aiString str;
    if (mat->GetTexture(type, index, &str) == AI_SUCCESS) {
        std::string filename = str.C_Str();
        // Remove path if present, keep only filename
        size_t lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = filename.substr(lastSlash + 1);
        }
        return filename;
    }
    return "";
}

Texture::Type Model::aiTextureTypeToTextureType(aiTextureType type) {
    switch (type) {
        case aiTextureType_DIFFUSE: return Texture::Type::DIFFUSE;
        case aiTextureType_SPECULAR: return Texture::Type::SPECULAR;
        case aiTextureType_HEIGHT: return Texture::Type::NORMAL;
        case aiTextureType_DISPLACEMENT: return Texture::Type::HEIGHT;
        case aiTextureType_EMISSIVE: return Texture::Type::EMISSIVE;
        default: return Texture::Type::DIFFUSE;
    }
}

void Model::render(std::shared_ptr<Shader> shader) const {
    for (const auto& mesh : m_meshes) {
        mesh->render(shader);
    }
}

std::shared_ptr<Model> Model::loadFromFileShared(const std::string& path) {
    auto model = std::make_shared<Model>();
    if (model->loadFromFile(path)) {
        return model;
    }
    return nullptr;
}

std::string Model::getSupportedFormats() {
    return "OBJ, FBX, GLTF/GLB, 3DS, DAE (Collada), PLY, STL, and many others supported by Assimp";
}

} // namespace IKore
