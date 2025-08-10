#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include "Logger.h"
#include "DeltaTimeDemo.h"
#include "Shader.h"
#include "Light.h"
#include "Texture.h"
#include "Model.h"

int main() {
    // Initialize logging system
    IKore::Logger::getInstance().initialize();
    LOG_INFO("Starting IKore Engine");

    if (!glfwInit()) {
        LOG_ERROR("Failed to initialize GLFW");
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    LOG_INFO("GLFW initialized successfully");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "IKore Engine", nullptr, nullptr);
    if (!window) {
        LOG_ERROR("Failed to create GLFW window");
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    LOG_INFO("Window created successfully");
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG_ERROR("Failed to initialize GLAD");
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }
    LOG_INFO("GLAD initialized successfully");

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    const double targetFPS = 60.0;
    const double targetFrameTime = 1.0 / targetFPS;

    // Initialize delta time demo
    IKore::DeltaTimeDemo deltaDemo;
    deltaDemo.initialize();

    // === Cube vertices with normals and texture coordinates ===
    float vertices[] = {
        // positions          // normals           // texture coords
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    std::string shaderError;
    auto shaderPtr = IKore::Shader::loadFromFilesCached("src/shaders/phong.vert", "src/shaders/phong.frag", shaderError);
    if(!shaderPtr){
        LOG_ERROR(std::string("Shader file load/compile failed: ") + shaderError);
    }

    // Setup lights
    IKore::DirectionalLight dirLight(glm::vec3(-0.2f, -1.0f, -0.3f));
    dirLight.ambient = glm::vec3(0.05f, 0.05f, 0.05f);
    dirLight.diffuse = glm::vec3(0.4f, 0.4f, 0.4f);
    dirLight.specular = glm::vec3(0.5f, 0.5f, 0.5f);

    IKore::PointLight pointLight(glm::vec3(1.2f, 1.0f, 2.0f));
    pointLight.ambient = glm::vec3(0.05f, 0.05f, 0.05f);
    pointLight.diffuse = glm::vec3(0.8f, 0.8f, 0.8f);
    pointLight.specular = glm::vec3(1.0f, 1.0f, 1.0f);

    // === Texture Loading ===
    IKore::TextureManager textureManager;
    
    // Load diffuse texture
    auto diffuseTexture = IKore::Texture::loadFromFileShared("assets/textures/colorful.png", IKore::Texture::Type::DIFFUSE);
    if (diffuseTexture) {
        textureManager.addTexture(diffuseTexture, "material.diffuse");
        LOG_INFO("Successfully loaded and added diffuse texture");
    } else {
        LOG_ERROR("Failed to load diffuse texture - falling back to color-only material");
    }

    // Load specular texture
    auto specularTexture = IKore::Texture::loadFromFileShared("assets/textures/specular.png", IKore::Texture::Type::SPECULAR);
    if (specularTexture) {
        textureManager.addTexture(specularTexture, "material.specular");
        LOG_INFO("Successfully loaded and added specular texture");
    } else {
        LOG_ERROR("Failed to load specular texture - falling back to color-only material");
    }

    // === Model Loading ===
    LOG_INFO("Loading 3D model...");
    IKore::Model cubeModel;
    bool modelLoaded = cubeModel.loadFromFile("assets/models/cube.obj");
    if (!modelLoaded || cubeModel.getMeshes().empty()) {
        LOG_ERROR("Failed to load model - falling back to primitive geometry");
    } else {
        LOG_INFO("Successfully loaded model with " + std::to_string(cubeModel.getMeshes().size()) + " meshes");
    }

    // Camera setup
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // Delta time tracking
    double lastFrameTime = glfwGetTime();
    double deltaTime = 0.0;
    
    // Frame timing statistics
    double frameTimeAccumulator = 0.0;
    int frameCount = 0;
    double statsTimer = 0.0;

    LOG_INFO("Starting main game loop with delta time calculation");

    while (!glfwWindowShouldClose(window)) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        // Calculate delta time using glfwGetTime() for precision
        double currentFrameTime = glfwGetTime();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;
        
        // Update statistics
        frameTimeAccumulator += deltaTime;
        frameCount++;
        statsTimer += deltaTime;
        
        // Log frame timing statistics every 2 seconds
        if (statsTimer >= 2.0) {
            double avgFrameTime = frameTimeAccumulator / frameCount;
            double avgFPS = 1.0 / avgFrameTime;
            LOG_INFO("Frame stats - Avg FPS: " + std::to_string(avgFPS) + 
                    ", Avg Frame Time: " + std::to_string(avgFrameTime * 1000.0) + "ms" +
                    ", Current Delta: " + std::to_string(deltaTime * 1000.0) + "ms");
            
            // Reset statistics
            frameTimeAccumulator = 0.0;
            frameCount = 0;
            statsTimer = 0.0;
        }

        // Input processing
        glfwPollEvents();

        // Update demo
        deltaDemo.update();
        deltaDemo.updateTestMovement();

        // Animate point light position
        pointLight.setPosition(glm::vec3(
            sin(currentFrameTime) * 2.0f,
            cos(currentFrameTime) * 1.0f,
            sin(currentFrameTime * 0.5f) * 2.0f + 2.0f
        ));

        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        if(shaderPtr) {
            shaderPtr->use();
            
            // Set up matrices
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::rotate(model, (float)currentFrameTime * glm::radians(50.0f), glm::vec3(0.5f, 1.0f, 0.0f));
            
            glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
            glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
            
            glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
            
            // Set matrices
            shaderPtr->setMat4("model", glm::value_ptr(model));
            shaderPtr->setMat4("view", glm::value_ptr(view));
            shaderPtr->setMat4("projection", glm::value_ptr(projection));
            shaderPtr->setMat3("normalMatrix", glm::value_ptr(normalMatrix));
            
            // Set camera position
            shaderPtr->setVec3("viewPos", cameraPos.x, cameraPos.y, cameraPos.z);
            
            // Bind textures and set material properties
            textureManager.bindAll(shaderPtr->id());
            
            // Set material properties (support both textured and non-textured materials)
            bool hasTextures = textureManager.getTextureCount() > 0;
            
            if (hasTextures) {
                // Using textures - set flags
                shaderPtr->setFloat("material.useDiffuseTexture", 1.0f);
                shaderPtr->setFloat("material.useSpecularTexture", 
                    textureManager.getTextureByType(IKore::Texture::Type::SPECULAR) ? 1.0f : 0.0f);
                
                // Set fallback colors for areas where texture might not cover
                shaderPtr->setVec3("material.ambient", 0.2f, 0.2f, 0.2f);
                shaderPtr->setVec3("material.diffuseColor", 1.0f, 1.0f, 1.0f);  // White to not tint texture
                shaderPtr->setVec3("material.specularColor", 0.5f, 0.5f, 0.5f);
            } else {
                // No textures - use colors only
                shaderPtr->setFloat("material.useDiffuseTexture", 0.0f);
                shaderPtr->setFloat("material.useSpecularTexture", 0.0f);
                shaderPtr->setVec3("material.ambient", 1.0f, 0.5f, 0.31f);
                shaderPtr->setVec3("material.diffuseColor", 1.0f, 0.5f, 0.31f);
                shaderPtr->setVec3("material.specularColor", 0.5f, 0.5f, 0.5f);
            }
            
            shaderPtr->setFloat("material.shininess", 32.0f);
            
            // Set directional light
            shaderPtr->setFloat("useDirLight", 1.0f);
            shaderPtr->setVec3("dirLight.direction", dirLight.direction.x, dirLight.direction.y, dirLight.direction.z);
            shaderPtr->setVec3("dirLight.ambient", dirLight.ambient.x, dirLight.ambient.y, dirLight.ambient.z);
            shaderPtr->setVec3("dirLight.diffuse", dirLight.diffuse.x, dirLight.diffuse.y, dirLight.diffuse.z);
            shaderPtr->setVec3("dirLight.specular", dirLight.specular.x, dirLight.specular.y, dirLight.specular.z);
            
            // Set point light
            shaderPtr->setFloat("numPointLights", 1.0f);
            shaderPtr->setVec3("pointLights[0].position", pointLight.position.x, pointLight.position.y, pointLight.position.z);
            shaderPtr->setVec3("pointLights[0].ambient", pointLight.ambient.x, pointLight.ambient.y, pointLight.ambient.z);
            shaderPtr->setVec3("pointLights[0].diffuse", pointLight.diffuse.x, pointLight.diffuse.y, pointLight.diffuse.z);
            shaderPtr->setVec3("pointLights[0].specular", pointLight.specular.x, pointLight.specular.y, pointLight.specular.z);
            shaderPtr->setFloat("pointLights[0].constant", pointLight.constant);
            shaderPtr->setFloat("pointLights[0].linear", pointLight.linear);
            shaderPtr->setFloat("pointLights[0].quadratic", pointLight.quadratic);
            
            // No spot lights for now
            shaderPtr->setFloat("numSpotLights", 0.0f);
            
            // Render models if available, otherwise fallback to primitive cube
            if (modelLoaded && !cubeModel.getMeshes().empty()) {
                // Render the loaded model
                cubeModel.render(shaderPtr);
            } else {
                // Fallback: render primitive cube
                glBindVertexArray(VAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
            }
            
            // Unbind textures
            textureManager.unbindAll();
        }

        glfwSwapBuffers(window);

        // Frame timing to cap FPS
        auto frameEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = frameEnd - frameStart;
        double sleepTime = targetFrameTime - elapsed.count();
        if (sleepTime > 0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
        }
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    LOG_INFO("Shutting down IKore Engine");
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
