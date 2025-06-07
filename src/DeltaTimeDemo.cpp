#include "DeltaTimeDemo.h"
#include <GLFW/glfw3.h>

namespace IKore {

DeltaTimeDemo::DeltaTimeDemo()
    : m_lastFrameTime(0.0)
    , m_deltaTime(0.0)
    , m_fpsAccumulator(0.0)
    , m_frameCount(0)
    , m_currentFPS(0.0)
    , m_testPosition(0.0)
    , m_testVelocity(100.0) // 100 units per second
    , m_initialized(false)
{
}

void DeltaTimeDemo::initialize() {
    m_lastFrameTime = glfwGetTime();
    m_deltaTime = 0.0;
    m_fpsAccumulator = 0.0;
    m_frameCount = 0;
    m_currentFPS = 0.0;
    m_initialized = true;
}

void DeltaTimeDemo::update() {
    if (!m_initialized) {
        initialize();
        return;
    }
    
    // Calculate delta time using glfwGetTime() for precision
    double currentFrameTime = glfwGetTime();
    m_deltaTime = currentFrameTime - m_lastFrameTime;
    m_lastFrameTime = currentFrameTime;
    
    // Update FPS calculation
    m_fpsAccumulator += m_deltaTime;
    m_frameCount++;
    
    // Update FPS every second
    if (m_fpsAccumulator >= 1.0) {
        m_currentFPS = m_frameCount / m_fpsAccumulator;
        m_fpsAccumulator = 0.0;
        m_frameCount = 0;
    }
}

double DeltaTimeDemo::getDeltaTime() const {
    return m_deltaTime;
}

double DeltaTimeDemo::getFPS() const {
    return m_currentFPS;
}

void DeltaTimeDemo::updateTestMovement() {
    // Frame-rate independent movement
    m_testPosition += m_testVelocity * m_deltaTime;
}

double DeltaTimeDemo::getTestPosition() const {
    return m_testPosition;
}

void DeltaTimeDemo::resetTest() {
    m_testPosition = 0.0;
    m_fpsAccumulator = 0.0;
    m_frameCount = 0;
    m_currentFPS = 0.0;
}

}