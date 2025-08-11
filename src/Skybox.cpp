#include "Skybox.h"
#include "Logger.h"
#include <stb_image.h>
#include <iostream>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>

namespace IKore {

    // Skybox cube vertices (positions only, no texture coordinates needed for cubemap)
    static const float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    Skybox::Skybox() 
        : m_VAO(0), m_VBO(0), m_textureID(0), m_shader(nullptr), 
          m_enabled(true), m_intensity(1.0f), m_initialized(false) {
    }

    Skybox::~Skybox() {
        cleanup();
    }

    bool Skybox::initialize(const std::vector<std::string>& faces) {
        if (m_initialized) {
            LOG_WARNING("Skybox already initialized, cleaning up previous resources");
            cleanup();
        }

        // Load shader
        std::string shaderError;
        m_shader = Shader::loadFromFilesCached("src/shaders/skybox.vert", "src/shaders/skybox.frag", shaderError);
        if (!m_shader) {
            LOG_ERROR("Failed to load skybox shader: " + shaderError);
            return false;
        }

        // Initialize OpenGL buffers
        initializeBuffers();

        // Load cubemap texture
        m_textureID = loadCubemapTexture(faces);
        if (m_textureID == 0) {
            LOG_ERROR("Failed to load skybox cubemap texture");
            cleanup();
            return false;
        }

        m_currentFaces = faces;
        m_initialized = true;
        
        LOG_INFO("Skybox initialized successfully with " + std::to_string(faces.size()) + " faces");
        return true;
    }

    bool Skybox::loadCubemap(const std::vector<std::string>& faces) {
        if (!m_initialized) {
            LOG_ERROR("Skybox not initialized, call initialize() first");
            return false;
        }

        // Delete old texture
        if (m_textureID != 0) {
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
        }

        // Load new cubemap texture
        m_textureID = loadCubemapTexture(faces);
        if (m_textureID == 0) {
            LOG_ERROR("Failed to load new skybox cubemap texture");
            return false;
        }

        m_currentFaces = faces;
        LOG_INFO("Skybox cubemap reloaded successfully");
        return true;
    }

    bool Skybox::initializeTestSkybox() {
        if (m_initialized) {
            LOG_WARNING("Skybox already initialized, cleaning up previous resources");
            cleanup();
        }

        // Load shader
        std::string shaderError;
        m_shader = Shader::loadFromFilesCached("src/shaders/skybox.vert", "src/shaders/skybox.frag", shaderError);
        if (!m_shader) {
            LOG_ERROR("Failed to load skybox shader: " + shaderError);
            return false;
        }

        // Initialize OpenGL buffers
        initializeBuffers();

        // Create procedural test cubemap
        m_textureID = createTestCubemap();
        if (m_textureID == 0) {
            LOG_ERROR("Failed to create test skybox cubemap");
            cleanup();
            return false;
        }

        m_currentFaces = {"test_right", "test_left", "test_top", "test_bottom", "test_front", "test_back"};
        m_initialized = true;
        
        LOG_INFO("Test skybox initialized successfully");
        return true;
    }

    bool Skybox::loadFromDirectory(const std::string& directory, const std::string& extension) {
        std::vector<std::string> faces = getStandardFaceNames(directory, extension);
        
        if (faces.size() != 6) {
            LOG_ERROR("Could not find all 6 skybox faces in directory: " + directory);
            return false;
        }

        if (!m_initialized) {
            return initialize(faces);
        } else {
            return loadCubemap(faces);
        }
    }

    void Skybox::render(const glm::mat4& view, const glm::mat4& projection) {
        if (!m_enabled || !isLoaded()) {
            return;
        }

        // Change depth function so depth test passes when values are equal to depth buffer's content
        glDepthFunc(GL_LEQUAL);
        
        m_shader->use();
        
        // Remove translation from the view matrix (only keep rotation)
        glm::mat4 skyboxView = glm::mat4(glm::mat3(view));
        
        // Set uniforms
        m_shader->setMat4("view", glm::value_ptr(skyboxView));
        m_shader->setMat4("projection", glm::value_ptr(projection));
        m_shader->setFloat("intensity", m_intensity);
        
        // Bind skybox VAO
        glBindVertexArray(m_VAO);
        
        // Bind cubemap texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
        m_shader->setInt("skybox", 0);
        
        // Render cube
        glDrawArrays(GL_TRIANGLES, 0, 36);
        
        // Unbind
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        
        // Set depth function back to default
        glDepthFunc(GL_LESS);
    }

    void Skybox::cleanup() {
        if (m_VAO != 0) {
            glDeleteVertexArrays(1, &m_VAO);
            m_VAO = 0;
        }
        
        if (m_VBO != 0) {
            glDeleteBuffers(1, &m_VBO);
            m_VBO = 0;
        }
        
        if (m_textureID != 0) {
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
        }
        
        m_shader.reset();
        m_initialized = false;
        m_currentFaces.clear();
        
        LOG_INFO("Skybox resources cleaned up");
    }

    void Skybox::initializeBuffers() {
        // Generate and bind VAO
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);

        // Position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    GLuint Skybox::loadCubemapTexture(const std::vector<std::string>& faces) {
        if (faces.size() != 6) {
            LOG_ERROR("Cubemap requires exactly 6 faces, got " + std::to_string(faces.size()));
            return 0;
        }

        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

        int width, height, nrChannels;
        for (unsigned int i = 0; i < faces.size(); i++) {
            unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
            if (data) {
                GLenum format;
                if (nrChannels == 1)
                    format = GL_RED;
                else if (nrChannels == 3)
                    format = GL_RGB;
                else if (nrChannels == 4)
                    format = GL_RGBA;
                else {
                    LOG_ERROR("Unsupported number of channels in skybox face: " + faces[i]);
                    stbi_image_free(data);
                    continue;
                }

                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                           0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                           
                LOG_INFO("Loaded skybox face " + std::to_string(i) + ": " + faces[i] + 
                        " (" + std::to_string(width) + "x" + std::to_string(height) + ")");
                stbi_image_free(data);
            } else {
                LOG_ERROR("Cubemap tex failed to load at path: " + faces[i]);
                stbi_image_free(data);
                glDeleteTextures(1, &textureID);
                return 0;
            }
        }

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        
        LOG_INFO("Cubemap texture created successfully with ID: " + std::to_string(textureID));
        return textureID;
    }

    GLuint Skybox::createTestCubemap() {
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

        // Define colors for each face: right, left, top, bottom, front, back
        std::vector<std::vector<unsigned char>> faceColors = {
            {255, 100, 100}, // right - red
            {100, 255, 100}, // left - green  
            {100, 100, 255}, // top - blue
            {255, 255, 100}, // bottom - yellow
            {255, 100, 255}, // front - magenta
            {100, 255, 255}  // back - cyan
        };

        int size = 256; // Simple 256x256 texture
        std::vector<unsigned char> faceData(size * size * 3);

        for (int face = 0; face < 6; face++) {
            // Fill the face with a gradient
            for (int y = 0; y < size; y++) {
                for (int x = 0; x < size; x++) {
                    int index = (y * size + x) * 3;
                    
                    // Create gradient effect
                    float gradientX = static_cast<float>(x) / size;
                    float gradientY = static_cast<float>(y) / size;
                    float intensity = 0.3f + 0.7f * (gradientX + gradientY) * 0.5f;
                    
                    faceData[index + 0] = static_cast<unsigned char>(faceColors[face][0] * intensity);
                    faceData[index + 1] = static_cast<unsigned char>(faceColors[face][1] * intensity);
                    faceData[index + 2] = static_cast<unsigned char>(faceColors[face][2] * intensity);
                }
            }

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 
                       0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, faceData.data());
                       
            LOG_INFO("Created test skybox face " + std::to_string(face) + " with color (" + 
                    std::to_string(faceColors[face][0]) + ", " + 
                    std::to_string(faceColors[face][1]) + ", " + 
                    std::to_string(faceColors[face][2]) + ")");
        }

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        
        LOG_INFO("Test cubemap texture created successfully with ID: " + std::to_string(textureID));
        return textureID;
    }

    std::vector<std::string> Skybox::getStandardFaceNames(const std::string& directory, 
                                                         const std::string& extension) {
        std::vector<std::string> standardNames = {
            "right", "left", "top", "bottom", "front", "back"
        };
        
        std::vector<std::string> faces;
        
        for (const auto& name : standardNames) {
            std::string path = directory + "/" + name + extension;
            if (std::filesystem::exists(path)) {
                faces.push_back(path);
                LOG_INFO("Found skybox face: " + path);
            } else {
                LOG_ERROR("Missing skybox face: " + path);
                faces.clear();
                break;
            }
        }
        
        return faces;
    }

} // namespace IKore