#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <filesystem>
#include <thread>

#include "Logger.h"
#include "Camera.h"
#include "CameraController.h"
#include "Shader.h"
#include "Texture.h"
#include "Model.h"
#include "Light.h"
#include "PostProcessor.h"
#include "DeltaTimeDemo.h"
#include "Skybox.h"
#include "ParticleSystem.h"
#include "ShadowMap.h"
#include "Frustum.h"
#include "Entity.h"
#include "EntityTypes.h"

// Forward declarations for GLFW callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

// Function to render scene objects (for both shadow pass and main rendering)
void renderSceneObjects(std::shared_ptr<IKore::Shader> shader, GLuint VAO, bool modelLoaded, 
                       const IKore::Model& cubeModel, double currentFrameTime);

// Global camera instance and controller
IKore::Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
IKore::CameraController cameraController(camera);

// Global post-processor pointer for keyboard callbacks
IKore::PostProcessor* g_postProcessor = nullptr;

// Global skybox pointer for keyboard callbacks  
IKore::Skybox* g_skybox = nullptr;

// Global particle system manager for keyboard callbacks
IKore::ParticleSystemManager* g_particleManager = nullptr;

// Global shadow map manager for keyboard callbacks
IKore::ShadowMapManager* g_shadowManager = nullptr;

// Global frustum culler for rendering optimization
IKore::FrustumCuller g_frustumCuller;
bool g_frustumCullingEnabled = true;

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
    } else {
        LOG_WARNING("Skybox initialization failed - skybox will be disabled");
        skybox.setEnabled(false);
    }
    
    g_skybox = &skybox; // Set global pointer for keyboard callbacks
    
    // === Particle System Initialization ===
    IKore::ParticleSystemManager particleManager;
    g_particleManager = &particleManager; // Set global pointer for keyboard callbacks
    
    // Create some default particle effects
    auto* fireSystem = particleManager.createFireEffect(glm::vec3(-2.0f, 0.0f, 0.0f));
    auto* smokeSystem = particleManager.createSmokeEffect(glm::vec3(2.0f, 0.0f, 0.0f));

    
    // Start the effects
    if (fireSystem) fireSystem->play();
    if (smokeSystem) smokeSystem->play();
    
    LOG_INFO("Particle systems initialized");
    LOG_INFO("Controls: Press 4 to toggle skybox, 5/6 to adjust intensity, 7 to toggle particles, 8 for fire, 9 for explosion, 0 for smoke, - for sparks");

    // === Entity System Initialization ===
    IKore::EntityManager& entityManager = IKore::getEntityManager();
    
    // Create some test entities to demonstrate the system
    auto testEntity1 = entityManager.createEntity<IKore::TestEntity>("Entity System Test 1");
    testEntity1->setLifetime(10.0f); // This entity will expire after 10 seconds
    
    auto testEntity2 = entityManager.createEntity<IKore::TestEntity>("Entity System Test 2");
    // testEntity2 has infinite lifetime
    
    auto gameObject1 = entityManager.createEntity<IKore::GameObject>("GameObject 1", glm::vec3(1.0f, 2.0f, 3.0f));
    gameObject1->setRotation(glm::vec3(45.0f, 0.0f, 0.0f));
    gameObject1->setScale(glm::vec3(2.0f, 1.0f, 1.0f));
    
    auto light1 = entityManager.createEntity<IKore::LightEntity>("Main Light", IKore::LightEntity::LightType::DIRECTIONAL);
    light1->setPosition(glm::vec3(0.0f, 5.0f, 0.0f));
    light1->setColor(glm::vec3(1.0f, 0.9f, 0.8f));
    light1->setIntensity(1.5f);
    
    auto camera1 = entityManager.createEntity<IKore::CameraEntity>("Entity Camera");
    camera1->setPosition(glm::vec3(0.0f, 0.0f, 5.0f));
    camera1->setFieldOfView(60.0f);
    
    // Initialize all entities
    entityManager.initializeAll();
    
    // Log entity system statistics
    auto stats = entityManager.getStats();
    LOG_INFO("Entity System initialized with " + std::to_string(stats.totalEntities) + " entities");
    LOG_INFO("Entity ID range: " + std::to_string(stats.lowestID) + " - " + std::to_string(stats.highestID));
    LOG_INFO("Controls: Press E to show entity stats, R to remove test entities, T to create new test entity");

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
    auto shaderPtr = IKore::Shader::loadFromFilesCached("src/shaders/phong_shadows.vert", "src/shaders/phong_shadows.frag", shaderError);
    if(!shaderPtr){
        LOG_ERROR(std::string("Shader file load/compile failed: ") + shaderError);
    }

    // Load shadow mapping shaders
    auto shadowShaderPtr = IKore::Shader::loadFromFilesCached("src/shaders/shadow_depth.vert", "src/shaders/shadow_depth.frag", shaderError);
    if(!shadowShaderPtr){
        LOG_ERROR(std::string("Shadow shader file load/compile failed: ") + shaderError);
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

    // === Shadow Mapping Initialization ===
    IKore::ShadowMapManager shadowManager;
    if (!shadowManager.initialize()) {
        LOG_ERROR("Failed to initialize shadow mapping system");
        return -1;
    }
    g_shadowManager = &shadowManager; // Set global pointer for keyboard callbacks
    
    // Create shadow map for directional light
    auto* dirShadowMap = shadowManager.createDirectionalShadowMap(0, 2048);
    if (!dirShadowMap) {
        LOG_WARNING("Failed to create directional shadow map - shadows will be disabled");
    } else {
        LOG_INFO("Directional shadow map created successfully");
    }
    
    // Create shadow map for point light
    auto* pointShadowMap = shadowManager.createPointShadowMap(1, 1024);
    if (!pointShadowMap) {
        LOG_WARNING("Failed to create point shadow map - point light shadows will be disabled");
    } else {
        LOG_INFO("Point light shadow map created successfully");
    }

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

        // Update particle systems
        particleManager.updateAll(static_cast<float>(deltaTime));

        // Update entity system
        entityManager.updateAll(static_cast<float>(deltaTime));

        // Update demo
        deltaDemo.update();
        deltaDemo.updateTestMovement();

        // Animate point light position
        pointLight.setPosition(glm::vec3(
            sin(currentFrameTime) * 2.0f,
            cos(currentFrameTime) * 1.0f,
            sin(currentFrameTime * 0.5f) * 2.0f + 2.0f
        ));

        // === SHADOW MAPPING PASSES ===
        
        // Update shadow maps with current light positions
        if (dirShadowMap) {
            dirShadowMap->setLightSpaceMatrix(dirLight.direction, glm::vec3(0.0f), 15.0f);
        }
        
        if (pointShadowMap) {
            pointShadowMap->setPointLightMatrices(pointLight.position, 25.0f);
        }
        
        // Render directional light shadow map
        if (dirShadowMap && shadowShaderPtr) {
            dirShadowMap->beginShadowPass();
            shadowShaderPtr->use();
            
            // Set light space matrix
            glm::mat4 lightSpaceMatrix = dirShadowMap->getLightSpaceMatrix();
            shadowShaderPtr->setMat4("lightSpaceMatrix", glm::value_ptr(lightSpaceMatrix));
            
            // Render scene objects to shadow map
            renderSceneObjects(shadowShaderPtr, VAO, modelLoaded, cubeModel, currentFrameTime);
            
            dirShadowMap->endShadowPass();
        }
        
        // Render point light shadow map (if point shadow shader is available)
        // Note: This would require the point shadow shader with geometry shader
        // For now, we'll focus on directional light shadows
        
        // === END SHADOW MAPPING PASSES ===

        // Begin post-processing frame (renders to framebuffer)
        postProcessor.beginFrame();
        
        // Get camera matrices for this frame
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix();
        
        // Update frustum for culling
        if (g_frustumCullingEnabled) {
            g_frustumCuller.updateFrustum(view, projection);
        }
        
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
            
            // Set directional light shadow mapping parameters
            if (dirShadowMap) {
                shaderPtr->setFloat("dirLight.castShadows", 1.0f);
                shaderPtr->setMat4("dirLight.lightSpaceMatrix", glm::value_ptr(dirShadowMap->getLightSpaceMatrix()));
                shaderPtr->setFloat("dirLight.shadowBias", dirShadowMap->getShadowBias());
                shaderPtr->setFloat("dirLight.softShadows", dirShadowMap->getSoftShadows() ? 1.0f : 0.0f);
                shaderPtr->setInt("dirLight.pcfKernelSize", dirShadowMap->getPCFKernelSize());
                
                // Bind shadow map texture
                dirShadowMap->bindShadowMap(15); // Use texture unit 15 for shadow map
                shaderPtr->setInt("dirLight.shadowMap", 15);
            } else {
                shaderPtr->setFloat("dirLight.castShadows", 0.0f);
            }
            
            // Set light space matrix for main shader (for FragPosLightSpace calculation)
            if (dirShadowMap) {
                shaderPtr->setMat4("lightSpaceMatrix", glm::value_ptr(dirShadowMap->getLightSpaceMatrix()));
            }
            
            // Set point light
            shaderPtr->setFloat("numPointLights", 1.0f);
            shaderPtr->setVec3("pointLights[0].position", pointLight.position.x, pointLight.position.y, pointLight.position.z);
            shaderPtr->setVec3("pointLights[0].ambient", pointLight.ambient.x, pointLight.ambient.y, pointLight.ambient.z);
            shaderPtr->setVec3("pointLights[0].diffuse", pointLight.diffuse.x, pointLight.diffuse.y, pointLight.diffuse.z);
            shaderPtr->setVec3("pointLights[0].specular", pointLight.specular.x, pointLight.specular.y, pointLight.specular.z);
            shaderPtr->setFloat("pointLights[0].constant", pointLight.constant);
            shaderPtr->setFloat("pointLights[0].linear", pointLight.linear);
            shaderPtr->setFloat("pointLights[0].quadratic", pointLight.quadratic);
            
            // Set point light shadow mapping parameters
            if (pointShadowMap) {
                shaderPtr->setFloat("pointLights[0].castShadows", 1.0f);
                shaderPtr->setFloat("pointLights[0].farPlane", 25.0f);
                shaderPtr->setFloat("pointLights[0].shadowBias", pointShadowMap->getShadowBias());
                shaderPtr->setFloat("pointLights[0].softShadows", pointShadowMap->getSoftShadows() ? 1.0f : 0.0f);
                
                // Bind shadow cubemap texture
                pointShadowMap->bindShadowMap(16); // Use texture unit 16 for point shadow cubemap
                shaderPtr->setInt("pointLights[0].shadowCubemap", 16);
            } else {
                shaderPtr->setFloat("pointLights[0].castShadows", 0.0f);
            }
            
            // No spot lights for now
            shaderPtr->setFloat("numSpotLights", 0.0f);
            
            // Render models if available, otherwise fallback to primitive cubes
            if (modelLoaded && !cubeModel.getMeshes().empty()) {
                // Create a larger grid of objects for better frustum culling demonstration
                std::vector<glm::vec3> cubePositions;
                
                // Generate a 5x5x5 grid of cubes (125 total objects)
                for (int x = -10; x <= 10; x += 4) {
                    for (int y = -10; y <= 10; y += 4) {
                        for (int z = -30; z <= 10; z += 8) {
                            cubePositions.push_back(glm::vec3(x, y, z));
                        }
                    }
                }
                
                int renderedObjects = 0;
                int culledObjects = 0;
                
                for (size_t i = 0; i < cubePositions.size(); i++) {
                    // Calculate individual model matrix for each cube
                    glm::mat4 instanceModel = glm::mat4(1.0f);
                    instanceModel = glm::translate(instanceModel, cubePositions[i]);
                    float angle = 20.0f * i + (float)currentFrameTime * 30.0f;
                    instanceModel = glm::rotate(instanceModel, glm::radians(angle), 
                                          glm::vec3(1.0f, 0.3f, 0.5f));
                    
                    // Perform frustum culling if enabled
                    bool shouldRender = true;
                    if (g_frustumCullingEnabled) {
                        shouldRender = g_frustumCuller.isVisible(cubeModel.getBoundingBox(), instanceModel);
                        if (!shouldRender) {
                            culledObjects++;
                            continue; // Skip rendering this object
                        }
                    }
                    
                    renderedObjects++;
                    glm::mat3 cubeNormalMatrix = glm::mat3(glm::transpose(glm::inverse(instanceModel)));
                    
                    // Update matrices for this cube
                    shaderPtr->setMat4("model", glm::value_ptr(instanceModel));
                    shaderPtr->setMat3("normalMatrix", glm::value_ptr(cubeNormalMatrix));
                    
                    // Render the model instance
                    cubeModel.render(shaderPtr);
                }
                
                // Log culling statistics periodically
                static double lastCullingLog = 0.0;
                if (currentFrameTime - lastCullingLog > 3.0) { // Every 3 seconds
                    if (g_frustumCullingEnabled && (renderedObjects + culledObjects) > 0) {
                        float cullingRatio = (float)culledObjects / (float)(renderedObjects + culledObjects);
                        LOG_INFO("Frustum culling stats - Total: " + std::to_string(renderedObjects + culledObjects) +
                                ", Rendered: " + std::to_string(renderedObjects) + 
                                ", Culled: " + std::to_string(culledObjects) + 
                                " (" + std::to_string(cullingRatio * 100.0f) + "% culled)");
                    }
                    lastCullingLog = currentFrameTime;
                }
            } else {
                // Fallback: render multiple primitive cubes with frustum culling
                std::vector<glm::vec3> cubePositions;
                
                // Generate a 5x5x5 grid of cubes (125 total objects) for primitive fallback
                for (int x = -10; x <= 10; x += 4) {
                    for (int y = -10; y <= 10; y += 4) {
                        for (int z = -30; z <= 10; z += 8) {
                            cubePositions.push_back(glm::vec3(x, y, z));
                        }
                    }
                }
                
                // Create a simple cube bounding box for primitive cubes
                IKore::BoundingBox cubeBounds(glm::vec3(-0.5f), glm::vec3(0.5f));
                
                int renderedObjects = 0;
                int culledObjects = 0;
                
                glBindVertexArray(VAO);
                for (size_t i = 0; i < cubePositions.size(); i++) {
                    // Calculate individual model matrix for each cube
                    glm::mat4 instanceModel = glm::mat4(1.0f);
                    instanceModel = glm::translate(instanceModel, cubePositions[i]);
                    float angle = 20.0f * i + (float)currentFrameTime * 30.0f;
                    instanceModel = glm::rotate(instanceModel, glm::radians(angle), 
                                          glm::vec3(1.0f, 0.3f, 0.5f));
                    
                    // Perform frustum culling if enabled
                    bool shouldRender = true;
                    if (g_frustumCullingEnabled) {
                        shouldRender = g_frustumCuller.isVisible(cubeBounds, instanceModel);
                        if (!shouldRender) {
                            culledObjects++;
                            continue; // Skip rendering this object
                        }
                    }
                    
                    renderedObjects++;
                    glm::mat3 cubeNormalMatrix = glm::mat3(glm::transpose(glm::inverse(instanceModel)));
                    
                    // Update matrices for this cube
                    shaderPtr->setMat4("model", glm::value_ptr(instanceModel));
                    shaderPtr->setMat3("normalMatrix", glm::value_ptr(cubeNormalMatrix));
                    
                    // Render the cube
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
                
                // Log culling statistics periodically
                static double lastPrimitiveCullingLog = 0.0;
                if (currentFrameTime - lastPrimitiveCullingLog > 3.0) { // Every 3 seconds
                    if (g_frustumCullingEnabled && (renderedObjects + culledObjects) > 0) {
                        float cullingRatio = (float)culledObjects / (float)(renderedObjects + culledObjects);
                        LOG_INFO("Primitive frustum culling stats - Total: " + std::to_string(renderedObjects + culledObjects) +
                                ", Rendered: " + std::to_string(renderedObjects) + 
                                ", Culled: " + std::to_string(culledObjects) + 
                                " (" + std::to_string(cullingRatio * 100.0f) + "% culled)");
                    }
                    lastPrimitiveCullingLog = currentFrameTime;
                }
                glBindVertexArray(0);
            }
            
            // Unbind textures
            textureManager.unbindAll();
        }

        // Render particle systems
        particleManager.renderAll(view, projection);

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

    // Cleanup entity system
    LOG_INFO("Cleaning up Entity System...");
    entityManager.cleanupAll();
    auto finalStats = entityManager.getStats();
    LOG_INFO("Final entity count: " + std::to_string(finalStats.totalEntities));
    entityManager.clear();

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
        else if (key == GLFW_KEY_7 && g_particleManager) {
            // Toggle particle systems
            g_particleManager->setGlobalEnabled(!g_particleManager->isGlobalEnabled());
            LOG_INFO("Particle systems " + std::string(g_particleManager->isGlobalEnabled() ? "enabled" : "disabled"));
        }
        else if (key == GLFW_KEY_8 && g_particleManager) {
            // Create fire effect at camera position
            glm::vec3 firePos = camera.getPosition() + camera.getFront() * 2.0f;
            g_particleManager->createFireEffect(firePos);
            LOG_INFO("Fire effect created at camera position");
        }
        else if (key == GLFW_KEY_9 && g_particleManager) {
            // Create explosion effect at camera position
            glm::vec3 explosionPos = camera.getPosition() + camera.getFront() * 3.0f;
            g_particleManager->createExplosionEffect(explosionPos);
            LOG_INFO("Explosion effect created at camera position");
        }
        else if (key == GLFW_KEY_0 && g_particleManager) {
            // Create smoke effect at camera position
            glm::vec3 smokePos = camera.getPosition() + camera.getFront() * 2.0f;
            g_particleManager->createSmokeEffect(smokePos);
            LOG_INFO("Smoke effect created at camera position");
        }
        else if (key == GLFW_KEY_MINUS && g_particleManager) {
            // Create sparks effect at camera position
            glm::vec3 sparksPos = camera.getPosition() + camera.getFront() * 2.0f;
            g_particleManager->createSparksEffect(sparksPos);
            LOG_INFO("Sparks effect created at camera position");
        }
        else if (key == GLFW_KEY_F1 && g_shadowManager) {
            // Toggle shadows globally
            g_shadowManager->setEnabled(!g_shadowManager->isEnabled());
            LOG_INFO("Shadow mapping " + std::string(g_shadowManager->isEnabled() ? "enabled" : "disabled"));
        }
        else if (key == GLFW_KEY_F2 && g_shadowManager) {
            // Cycle shadow quality (Low, Medium, High)
            static int qualityLevel = 1; // 0=Low, 1=Medium, 2=High
            qualityLevel = (qualityLevel + 1) % 3;
            float quality = qualityLevel == 0 ? 0.3f : (qualityLevel == 1 ? 0.6f : 1.0f);
            g_shadowManager->setShadowQuality(quality);
            std::string qualityName = qualityLevel == 0 ? "Low" : (qualityLevel == 1 ? "Medium" : "High");
            LOG_INFO("Shadow quality set to: " + qualityName);
        }
        else if (key == GLFW_KEY_C) {
            // Toggle frustum culling
            g_frustumCullingEnabled = !g_frustumCullingEnabled;
            LOG_INFO("Frustum culling " + std::string(g_frustumCullingEnabled ? "enabled" : "disabled"));
        }
        else if (key == GLFW_KEY_E) {
            // Show entity system statistics
            IKore::EntityManager& entityMgr = IKore::getEntityManager();
            auto stats = entityMgr.getStats();
            LOG_INFO("Entity System Statistics:");
            LOG_INFO("  Total entities: " + std::to_string(stats.totalEntities));
            LOG_INFO("  Valid entities: " + std::to_string(stats.validEntities));
            LOG_INFO("  Invalid entities: " + std::to_string(stats.invalidEntities));
            LOG_INFO("  ID range: " + std::to_string(stats.lowestID) + " - " + std::to_string(stats.highestID));
            
            // List all entities by type
            auto gameObjects = entityMgr.getEntitiesOfType<IKore::GameObject>();
            auto lights = entityMgr.getEntitiesOfType<IKore::LightEntity>();
            auto cameras = entityMgr.getEntitiesOfType<IKore::CameraEntity>();
            auto testEntities = entityMgr.getEntitiesOfType<IKore::TestEntity>();
            
            LOG_INFO("  GameObjects: " + std::to_string(gameObjects.size()));
            LOG_INFO("  Lights: " + std::to_string(lights.size()));
            LOG_INFO("  Cameras: " + std::to_string(cameras.size()));
            LOG_INFO("  Test entities: " + std::to_string(testEntities.size()));
        }
        else if (key == GLFW_KEY_R) {
            // Remove test entities
            IKore::EntityManager& entityMgr = IKore::getEntityManager();
            auto testEntities = entityMgr.getEntitiesOfType<IKore::TestEntity>();
            size_t removed = 0;
            
            for (auto& entity : testEntities) {
                if (entityMgr.removeEntity(entity)) {
                    removed++;
                }
            }
            
            LOG_INFO("Removed " + std::to_string(removed) + " test entities");
        }
        else if (key == GLFW_KEY_T) {
            // Create new test entity
            IKore::EntityManager& entityMgr = IKore::getEntityManager();
            static int testCounter = 0;
            testCounter++;
            
            auto newEntity = entityMgr.createEntity<IKore::TestEntity>("Dynamic Test Entity " + std::to_string(testCounter));
            newEntity->setLifetime(5.0f + (testCounter % 5)); // Varying lifetimes
            newEntity->initialize();
            
            LOG_INFO("Created new test entity with ID: " + std::to_string(newEntity->getID()));
        }
    }
}

// Function to render scene objects (for both shadow pass and main rendering)
void renderSceneObjects(std::shared_ptr<IKore::Shader> shader, GLuint VAO, bool modelLoaded, 
                       const IKore::Model& cubeModel, double currentFrameTime) {
    // Create a larger grid of objects for better frustum culling demonstration
    std::vector<glm::vec3> cubePositions;
    
    // Generate a 5x5x5 grid of cubes (125 total objects)
    for (int x = -10; x <= 10; x += 4) {
        for (int y = -10; y <= 10; y += 4) {
            for (int z = -30; z <= 10; z += 8) {
                cubePositions.push_back(glm::vec3(x, y, z));
            }
        }
    }
    
    // Render models if available, otherwise fallback to primitive cubes
    if (modelLoaded && !cubeModel.getMeshes().empty()) {
        for (size_t i = 0; i < cubePositions.size(); i++) {
            // Calculate individual model matrix for each cube
            glm::mat4 instanceModel = glm::mat4(1.0f);
            instanceModel = glm::translate(instanceModel, cubePositions[i]);
            float angle = 20.0f * i + (float)currentFrameTime * 30.0f;
            instanceModel = glm::rotate(instanceModel, glm::radians(angle), 
                                  glm::vec3(1.0f, 0.3f, 0.5f));
            
            glm::mat3 cubeNormalMatrix = glm::mat3(glm::transpose(glm::inverse(instanceModel)));
            
            // Set matrices for this instance
            shader->setMat4("model", glm::value_ptr(instanceModel));
            shader->setMat3("normalMatrix", glm::value_ptr(cubeNormalMatrix));
            
            // Render the model
            cubeModel.render(shader);
        }
    } else {
        // Fallback: render primitive cubes using VAO
        glBindVertexArray(VAO);
        
        for (size_t i = 0; i < cubePositions.size(); i++) {
            // Calculate individual model matrix for each cube
            glm::mat4 instanceModel = glm::mat4(1.0f);
            instanceModel = glm::translate(instanceModel, cubePositions[i]);
            float angle = 20.0f * i + (float)currentFrameTime * 30.0f;
            instanceModel = glm::rotate(instanceModel, glm::radians(angle), 
                                  glm::vec3(1.0f, 0.3f, 0.5f));
            
            glm::mat3 cubeNormalMatrix = glm::mat3(glm::transpose(glm::inverse(instanceModel)));
            
            // Set matrices for this instance
            shader->setMat4("model", glm::value_ptr(instanceModel));
            shader->setMat3("normalMatrix", glm::value_ptr(cubeNormalMatrix));
            
            // Draw the cube
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        
        glBindVertexArray(0);
    }
}
