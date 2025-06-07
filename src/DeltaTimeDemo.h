#pragma once

namespace IKore {

class DeltaTimeDemo {
public:
    DeltaTimeDemo();
    
    // Initialize delta time tracking
    void initialize();
    
    // Update delta time each frame - call this at the beginning of each frame
    void update();
    
    // Get the current delta time in seconds
    double getDeltaTime() const;
    
    // Get the current FPS
    double getFPS() const;
    
    // Test frame-rate independent movement
    void updateTestMovement();
    
    // Get test position for demonstration
    double getTestPosition() const;
    
    // Reset the test
    void resetTest();

private:
    double m_lastFrameTime;
    double m_deltaTime;
    double m_fpsAccumulator;
    int m_frameCount;
    double m_currentFPS;
    
    // Test variables for frame-rate independent movement
    double m_testPosition;
    double m_testVelocity; // units per second
    bool m_initialized;
};

}