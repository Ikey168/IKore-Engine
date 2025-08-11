#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <memory>
#include "Shader.h"

namespace IKore {

    class Skybox {
    public:
        // Constructor
        Skybox();
        
        // Destructor
        ~Skybox();

        // Initialize skybox with cubemap faces
        // faces should be in order: right, left, top, bottom, front, back
        bool initialize(const std::vector<std::string>& faces);
        
        // Load a new cubemap (supports dynamic skybox changes)
        bool loadCubemap(const std::vector<std::string>& faces);
        
        // Load cubemap from a directory with standard naming convention
        // Looks for: right.jpg, left.jpg, top.jpg, bottom.jpg, front.jpg, back.jpg
        bool loadFromDirectory(const std::string& directory, const std::string& extension = ".jpg");
        
        // Initialize with procedural test skybox (colored faces)
        bool initializeTestSkybox();
        
        // Render the skybox
        void render(const glm::mat4& view, const glm::mat4& projection);
        
        // Enable/disable skybox rendering
        void setEnabled(bool enabled) { m_enabled = enabled; }
        bool isEnabled() const { return m_enabled; }
        
        // Set skybox brightness/intensity
        void setIntensity(float intensity) { m_intensity = intensity; }
        float getIntensity() const { return m_intensity; }
        
        // Get cubemap texture ID
        GLuint getTextureID() const { return m_textureID; }
        
        // Check if skybox is properly loaded
        bool isLoaded() const { return m_textureID != 0 && m_shader != nullptr; }
        
        // Cleanup resources
        void cleanup();

    private:
        // OpenGL resources
        GLuint m_VAO;
        GLuint m_VBO;
        GLuint m_textureID;
        
        // Shader for skybox rendering
        std::shared_ptr<Shader> m_shader;
        
        // Skybox properties
        bool m_enabled;
        float m_intensity;
        bool m_initialized;
        
        // Currently loaded faces (for debugging/info)
        std::vector<std::string> m_currentFaces;
        
        // Initialize OpenGL resources
        void initializeBuffers();
        
        // Load cubemap texture from file paths
        GLuint loadCubemapTexture(const std::vector<std::string>& faces);
        
        // Create procedural test cubemap
        GLuint createTestCubemap();
        
        // Helper function to get standard face filenames
        std::vector<std::string> getStandardFaceNames(const std::string& directory, 
                                                     const std::string& extension);
    };

} // namespace IKore