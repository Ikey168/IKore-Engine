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

    // === Cube vertices with normals ===
    float vertices[] = {
        // positions          // normals
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

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
<<<<<<< HEAD
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
            
            // Set material properties
            shaderPtr->setVec3("material.ambient", 1.0f, 0.5f, 0.31f);
            shaderPtr->setVec3("material.diffuse", 1.0f, 0.5f, 0.31f);
            shaderPtr->setVec3("material.specular", 0.5f, 0.5f, 0.5f);
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
            
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
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
