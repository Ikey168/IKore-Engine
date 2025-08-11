#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <algorithm>
#include "Logger.h"
#include "DeltaTimeDemo.h"
#include "Shader.h"
#include "Light.h"
#include "Texture.h"
#include "Model.h"
#include "PostProcessor.h"
#include "Camera.h"
#include "CameraController.h"
#include "Skybox.h"

// Global camera instance and controller
IKore::Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
IKore::CameraController cameraController(camera);

// Global post-processor pointer for keyboard callbacks
IKore::PostProcessor* g_postProcessor = nullptr;

// Global skybox pointer for keyboard callbacks  
IKore::Skybox* g_skybox = nullptr;

// Mouse tracking variables
bool firstMouse = true;
float lastX = 1280.0f / 2.0f;
float lastY = 720.0f / 2.0f;

// Function declarations
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

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

    // Set up all callbacks
    glfwSetKeyCallback(window, key_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Capture mouse cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Set initial camera aspect ratio
    camera.setAspectRatio(1280.0f / 720.0f);

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

    // === Post-Processing Initialization ===
    IKore::PostProcessor postProcessor;
    postProcessor.initialize(1280, 720);
    g_postProcessor = &postProcessor; // Set global pointer for keyboard callbacks
    
    // Configure post-processing effects
    if (auto* bloom = postProcessor.getBloomEffect()) {
        bloom->setThreshold(1.0f);
        bloom->setIntensity(0.5f);
        bloom->setBlurPasses(5);
        bloom->setEnabled(true);
    }
    
    if (auto* fxaa = postProcessor.getFXAAEffect()) {
        fxaa->setEnabled(true);
    }
    
    if (auto* ssao = postProcessor.getSSAOEffect()) {
        ssao->setEnabled(false); // Disabled for now since we don't have G-buffer
    }
    
    LOG_INFO("Post-processing effects initialized");
    LOG_INFO("Controls: Press 1 to toggle Bloom, 2 to toggle FXAA, 3 to toggle SSAO");

    // === Skybox Initialization ===
    IKore::Skybox skybox;
    
    // Try to load skybox from directory first (if textures exist)
    bool skyboxLoaded = false;
    if (std::filesystem::exists("assets/skybox/space")) {
        skyboxLoaded = skybox.loadFromDirectory("assets/skybox/space", ".jpg");
        if (!skyboxLoaded) {
            skyboxLoaded = skybox.loadFromDirectory("assets/skybox/space", ".png");
        }
    }
    
    // Fallback: Create procedural skybox if no textures found
    if (!skyboxLoaded) {
        LOG_INFO("No skybox textures found, creating procedural test skybox");
        skyboxLoaded = skybox.initializeTestSkybox();
    }
    
    if (skyboxLoaded) {
        LOG_INFO("Skybox initialized successfully");
        LOG_INFO("Controls: Press 4 to toggle skybox, 5/6 to adjust intensity");
    } else {
        LOG_WARNING("Skybox initialization failed - skybox will be disabled");
        skybox.setEnabled(false);
    }
    
    // Set global skybox pointer for keyboard callbacks
    g_skybox = &skybox;

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
            
            // Log camera status every 10 seconds
            static int logCounter = 0;
            logCounter++;
            if (logCounter >= 5) { // Every 10 seconds (5 * 2 second intervals)
                std::string cameraStatus = cameraController.getCameraStatus();
                LOG_INFO("Camera Status Update:\n" + cameraStatus);
                logCounter = 0;
            }
            
            // Reset statistics
            frameTimeAccumulator = 0.0;
            frameCount = 0;
            statsTimer = 0.0;
        }

        // Input processing
        glfwPollEvents();
        cameraController.processInput(window, static_cast<float>(deltaTime));

        // Update demo
        deltaDemo.update();
        deltaDemo.updateTestMovement();

        // Animate point light position
        pointLight.setPosition(glm::vec3(
            sin(currentFrameTime) * 2.0f,
            cos(currentFrameTime) * 1.0f,
            sin(currentFrameTime * 0.5f) * 2.0f + 2.0f
        ));

        // Begin post-processing frame (renders to framebuffer)
        postProcessor.beginFrame();
        
        // Get camera matrices for this frame
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix();
        
        // Render skybox first (as background)
        skybox.render(view, projection);
        
        if(shaderPtr) {
            shaderPtr->use();
            
            // Set up matrices using Camera class
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::rotate(model, (float)currentFrameTime * glm::radians(50.0f), glm::vec3(0.5f, 1.0f, 0.0f));
            
            glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
            
            // Set matrices
            shaderPtr->setMat4("model", glm::value_ptr(model));
            shaderPtr->setMat4("view", glm::value_ptr(view));
            shaderPtr->setMat4("projection", glm::value_ptr(projection));
            shaderPtr->setMat3("normalMatrix", glm::value_ptr(normalMatrix));
            
            // Set camera position
            glm::vec3 cameraPos = camera.getPosition();
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
            
            // Render models if available, otherwise fallback to primitive cubes
            if (modelLoaded && !cubeModel.getMeshes().empty()) {
                // Render multiple model instances at different positions
                glm::vec3 cubePositions[] = {
                    glm::vec3( 0.0f,  0.0f,  0.0f),
                    glm::vec3( 2.0f,  5.0f, -15.0f),
                    glm::vec3(-1.5f, -2.2f, -2.5f),
                    glm::vec3(-3.8f, -2.0f, -12.3f),
                    glm::vec3( 2.4f, -0.4f, -3.5f),
                    glm::vec3(-1.7f,  3.0f, -7.5f),
                    glm::vec3( 1.3f, -2.0f, -2.5f),
                    glm::vec3( 1.5f,  2.0f, -2.5f),
                    glm::vec3( 1.5f,  0.2f, -1.5f),
                    glm::vec3(-1.3f,  1.0f, -1.5f)
                };
                
                for (size_t i = 0; i < 10; i++) {
                    // Calculate individual model matrix for each cube
                    glm::mat4 instanceModel = glm::mat4(1.0f);
                    instanceModel = glm::translate(instanceModel, cubePositions[i]);
                    float angle = 20.0f * i + (float)currentFrameTime * 30.0f;
                    instanceModel = glm::rotate(instanceModel, glm::radians(angle), 
                                          glm::vec3(1.0f, 0.3f, 0.5f));
                    
                    glm::mat3 cubeNormalMatrix = glm::mat3(glm::transpose(glm::inverse(instanceModel)));
                    
                    // Update matrices for this cube
                    shaderPtr->setMat4("model", glm::value_ptr(instanceModel));
                    shaderPtr->setMat3("normalMatrix", glm::value_ptr(cubeNormalMatrix));
                    
                    // Render the model instance
                    cubeModel.render(shaderPtr);
                }
            } else {
                // Fallback: render multiple primitive cubes
                glm::vec3 cubePositions[] = {
                    glm::vec3( 0.0f,  0.0f,  0.0f),
                    glm::vec3( 2.0f,  5.0f, -15.0f),
                    glm::vec3(-1.5f, -2.2f, -2.5f),
                    glm::vec3(-3.8f, -2.0f, -12.3f),
                    glm::vec3( 2.4f, -0.4f, -3.5f),
                    glm::vec3(-1.7f,  3.0f, -7.5f),
                    glm::vec3( 1.3f, -2.0f, -2.5f),
                    glm::vec3( 1.5f,  2.0f, -2.5f),
                    glm::vec3( 1.5f,  0.2f, -1.5f),
                    glm::vec3(-1.3f,  1.0f, -1.5f)
                };
                
                glBindVertexArray(VAO);
                for (size_t i = 0; i < 10; i++) {
                    // Calculate individual model matrix for each cube
                    glm::mat4 instanceModel = glm::mat4(1.0f);
                    instanceModel = glm::translate(instanceModel, cubePositions[i]);
                    float angle = 20.0f * i + (float)currentFrameTime * 30.0f;
                    instanceModel = glm::rotate(instanceModel, glm::radians(angle), 
                                          glm::vec3(1.0f, 0.3f, 0.5f));
                    
                    glm::mat3 cubeNormalMatrix = glm::mat3(glm::transpose(glm::inverse(instanceModel)));
                    
                    // Update matrices for this cube
                    shaderPtr->setMat4("model", glm::value_ptr(instanceModel));
                    shaderPtr->setMat3("normalMatrix", glm::value_ptr(cubeNormalMatrix));
                    
                    // Render the cube
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
                glBindVertexArray(0);
            }
            
            // Unbind textures
            textureManager.unbindAll();
        }

        // End post-processing frame (applies effects and renders to screen)
        postProcessor.endFrame();

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

// Mouse callback function
void mouse_callback(GLFWwindow* /*window*/, double xpos, double ypos) {
    if (!cameraController.isMouseLookEnabled()) {
        return;
    }

    if (firstMouse) {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos) - lastX;
    float yoffset = lastY - static_cast<float>(ypos); // Reversed since y-coordinates go from bottom to top

    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    camera.processMouseMovement(xoffset, yoffset);
}

// Scroll callback function
void scroll_callback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset) {
    camera.processMouseScroll(static_cast<float>(yoffset));
}

// Framebuffer size callback function
void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);
    camera.setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    if (g_postProcessor) {
        g_postProcessor->resize(width, height);
    }
}

// Keyboard callback for toggling post-processing effects
void key_callback(GLFWwindow* /*window*/, int key, int /*scancode*/, int action, int /*mods*/) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_1 && g_postProcessor) {
            // Toggle Bloom
            auto* bloom = g_postProcessor->getBloomEffect();
            if (bloom) {
                bloom->setEnabled(!bloom->isEnabled());
                LOG_INFO("Bloom effect " + std::string(bloom->isEnabled() ? "enabled" : "disabled"));
            }
        }
        else if (key == GLFW_KEY_2 && g_postProcessor) {
            // Toggle FXAA
            auto* fxaa = g_postProcessor->getFXAAEffect();
            if (fxaa) {
                fxaa->setEnabled(!fxaa->isEnabled());
                LOG_INFO("FXAA effect " + std::string(fxaa->isEnabled() ? "enabled" : "disabled"));
            }
        }
        else if (key == GLFW_KEY_3 && g_postProcessor) {
            // Toggle SSAO
            auto* ssao = g_postProcessor->getSSAOEffect();
            if (ssao) {
                ssao->setEnabled(!ssao->isEnabled());
                LOG_INFO("SSAO effect " + std::string(ssao->isEnabled() ? "enabled" : "disabled"));
            }
        }
        else if (key == GLFW_KEY_4 && g_skybox) {
            // Toggle Skybox
            g_skybox->setEnabled(!g_skybox->isEnabled());
            LOG_INFO("Skybox " + std::string(g_skybox->isEnabled() ? "enabled" : "disabled"));
        }
        else if (key == GLFW_KEY_5 && g_skybox) {
            // Decrease skybox intensity
            float currentIntensity = g_skybox->getIntensity();
            float newIntensity = std::max(0.1f, currentIntensity - 0.1f);
            g_skybox->setIntensity(newIntensity);
            LOG_INFO("Skybox intensity decreased to: " + std::to_string(newIntensity));
        }
        else if (key == GLFW_KEY_6 && g_skybox) {
            // Increase skybox intensity
            float currentIntensity = g_skybox->getIntensity();
            float newIntensity = std::min(3.0f, currentIntensity + 0.1f);
            g_skybox->setIntensity(newIntensity);
            LOG_INFO("Skybox intensity increased to: " + std::to_string(newIntensity));
        }
    }
}
