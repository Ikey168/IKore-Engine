#include "PerformanceOverlay.h"
#include "Logger.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <sys/resource.h>
#include <fstream>
#endif

namespace IKore {

    // Simple bitmap font for text rendering (8x8 pixel characters)
    // This is a minimal implementation - in a full engine you'd use a proper font library
    const unsigned char FONT_DATA[256][8] = {
        // ASCII 32 (space) to 126 (~)
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 32: space
        {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // 33: !
        {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 34: "
        {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // 35: #
        {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, // 36: $
        {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, // 37: %
        {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, // 38: &
        {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // 39: '
        {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, // 40: (
        {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, // 41: )
        {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // 42: *
        {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}, // 43: +
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x06, 0x00}, // 44: ,
        {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, // 45: -
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // 46: .
        {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, // 47: /
        {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, // 48: 0
        {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, // 49: 1
        {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, // 50: 2
        {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, // 51: 3
        {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, // 52: 4
        {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, // 53: 5
        {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, // 54: 6
        {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, // 55: 7
        {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, // 56: 8
        {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, // 57: 9
        {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // 58: :
        {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x06, 0x00}, // 59: ;
        {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, // 60: <
        {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00}, // 61: =
        {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // 62: >
        {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, // 63: ?
        {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, // 64: @
        {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, // 65: A
        {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, // 66: B
        {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, // 67: C
        {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, // 68: D
        {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, // 69: E
        {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, // 70: F
        {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, // 71: G
        {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, // 72: H
        {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // 73: I
        {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, // 74: J
        {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, // 75: K
        {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, // 76: L
        {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, // 77: M
        {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, // 78: N
        {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // 79: O
        {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, // 80: P
        {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, // 81: Q
        {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, // 82: R
        {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, // 83: S
        {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // 84: T
        {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, // 85: U
        {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // 86: V
        {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // 87: W
        {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, // 88: X
        {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, // 89: Y
        {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, // 90: Z
    };

    PerformanceOverlay& PerformanceOverlay::getInstance() {
        static PerformanceOverlay instance;
        return instance;
    }

    PerformanceOverlay& getPerformanceOverlay() {
        return PerformanceOverlay::getInstance();
    }

    bool PerformanceOverlay::initialize(int windowWidth, int windowHeight) {
        if (m_initialized) {
            return true;
        }

        m_windowWidth = windowWidth;
        m_windowHeight = windowHeight;

        // Initialize frame time samples
        m_frameTimes.resize(FRAME_TIME_SAMPLES, 0.0f);

        // Initialize timing
        m_lastStatsUpdate = std::chrono::steady_clock::now();

        // Initialize OpenGL resources
        if (!initializeTextRendering()) {
            LOG_ERROR("Failed to initialize text rendering for performance overlay");
            return false;
        }

        // Initialize GPU timing if supported - simplified for compatibility
        m_gpuTimingSupported = false; // Disable advanced GPU timing for now
        LOG_INFO("GPU timing queries disabled for compatibility");

        m_initialized = true;
        LOG_INFO("Performance overlay initialized successfully");
        return true;
    }

    void PerformanceOverlay::shutdown() {
        if (!m_initialized) {
            return;
        }

        // Clean up OpenGL resources
        if (m_quadVAO) {
            glDeleteVertexArrays(1, &m_quadVAO);
            m_quadVAO = 0;
        }
        if (m_quadVBO) {
            glDeleteBuffers(1, &m_quadVBO);
            m_quadVBO = 0;
        }
        if (m_fontTexture) {
            glDeleteTextures(1, &m_fontTexture);
            m_fontTexture = 0;
        }
        if (m_textShaderProgram) {
            glDeleteProgram(m_textShaderProgram);
            m_textShaderProgram = 0;
        }
        if (m_quadShaderProgram) {
            glDeleteProgram(m_quadShaderProgram);
            m_quadShaderProgram = 0;
        }
        if (m_gpuTimingSupported) {
            glDeleteQueries(2, m_gpuQueries);
        }

        m_initialized = false;
        LOG_INFO("Performance overlay shutdown complete");
    }

    void PerformanceOverlay::update(float deltaTime) {
        if (!m_initialized) {
            return;
        }

        // Update FPS statistics
        updateFPSStats(deltaTime);

        // Update system statistics periodically
        auto now = std::chrono::steady_clock::now();
        float timeSinceLastUpdate = std::chrono::duration<float>(now - m_lastStatsUpdate).count();
        
        if (timeSinceLastUpdate >= STATS_UPDATE_INTERVAL) {
            updateSystemStats();
            updateGPUStats();
            calculatePerformanceIndicators();
            m_lastStatsUpdate = now;
        }

        // Reset frame statistics for next frame
        resetFrameStats();
    }

    void PerformanceOverlay::render() {
        if (!m_initialized || !m_config.enabled) {
            return;
        }

        // Save OpenGL state
        GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
        GLboolean blending = glIsEnabled(GL_BLEND);
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);

        // Set up rendering state
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glViewport(0, 0, m_windowWidth, m_windowHeight);

        // Calculate overlay dimensions
        float overlayHeight = 12 * m_config.lineSpacing + 2 * m_config.padding;
        if (m_config.showAdvancedStats) {
            overlayHeight += 8 * m_config.lineSpacing; // Additional lines for advanced stats
        }

        // Render background
        renderBackground(m_config.position.x, m_config.position.y, 
                        m_config.width, overlayHeight, m_config.backgroundColor);

        // Render text
        float currentY = m_config.position.y + m_config.padding;
        
        // FPS and frame timing
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);
        
        oss.str("");
        oss << "FPS: " << m_stats.currentFPS;
        renderText(oss.str(), m_config.position.x + m_config.padding, currentY, 
                  m_stats.isPerformanceGood ? m_config.goodPerformanceColor : m_config.badPerformanceColor);
        currentY += m_config.lineSpacing;

        oss.str("");
        oss << "Frame Time: " << m_stats.frameTime << " ms";
        renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
        currentY += m_config.lineSpacing;

        oss.str("");
        oss << "Avg FPS: " << m_stats.averageFPS;
        renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
        currentY += m_config.lineSpacing;

        oss.str("");
        oss << "Min/Max: " << m_stats.minFrameTime << "/" << m_stats.maxFrameTime << " ms";
        renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
        currentY += m_config.lineSpacing;

        // Memory statistics
        if (m_config.showMemoryStats) {
            oss.str("");
            oss << "Memory: " << m_stats.memoryUsage << " MB";
            renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
            currentY += m_config.lineSpacing;

            oss.str("");
            oss << std::setprecision(1) << "CPU Usage: " << m_stats.cpuUsage << "%";
            renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
            currentY += m_config.lineSpacing;
        }

        // GPU statistics
        if (m_config.showGpuStats && m_gpuTimingSupported) {
            oss.str("");
            oss << "GPU Time: " << m_stats.gpuFrameTime << " ms";
            renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
            currentY += m_config.lineSpacing;
        }

        // Rendering statistics
        if (m_config.showRenderStats) {
            oss.str("");
            oss << "Draw Calls: " << m_stats.drawCalls;
            renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
            currentY += m_config.lineSpacing;

            oss.str("");
            oss << "Triangles: " << m_stats.triangles;
            renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
            currentY += m_config.lineSpacing;

            oss.str("");
            oss << "Entities: " << m_stats.activeEntities << "/" << m_stats.entityCount;
            renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
            currentY += m_config.lineSpacing;
        }

        // Advanced statistics
        if (m_config.showAdvancedStats) {
            currentY += m_config.lineSpacing * 0.5f; // Extra spacing

            oss.str("");
            oss << "=== Advanced Stats ===";
            renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
            currentY += m_config.lineSpacing;

            oss.str("");
            oss << "Frame Variance: " << std::setprecision(3) << m_stats.frameTimeVariance;
            renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
            currentY += m_config.lineSpacing;

            oss.str("");
            oss << "Peak Memory: " << m_stats.peakMemoryUsage << " MB";
            renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
            currentY += m_config.lineSpacing;

            oss.str("");
            oss << "Components: " << m_stats.componentCount;
            renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
            currentY += m_config.lineSpacing;

            if (m_gpuTimingSupported && m_stats.totalGpuMemory > 0) {
                oss.str("");
                oss << "GPU Memory: " << m_stats.gpuMemoryUsage << "/" << m_stats.totalGpuMemory << " MB";
                renderText(oss.str(), m_config.position.x + m_config.padding, currentY, m_config.textColor);
                currentY += m_config.lineSpacing;
            }
        }

        // Restore OpenGL state
        if (depthTest) glEnable(GL_DEPTH_TEST);
        if (!blending) glDisable(GL_BLEND);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }

    void PerformanceOverlay::toggle() {
        m_config.enabled = !m_config.enabled;
        LOG_INFO(m_config.enabled ? "Performance overlay enabled" : "Performance overlay disabled");
    }

    void PerformanceOverlay::show() {
        m_config.enabled = true;
    }

    void PerformanceOverlay::hide() {
        m_config.enabled = false;
    }

    void PerformanceOverlay::setPosition(float x, float y) {
        m_config.position = {x, y};
    }

    void PerformanceOverlay::setColors(const glm::vec4& background, const glm::vec4& text) {
        m_config.backgroundColor = background;
        m_config.textColor = text;
    }

    void PerformanceOverlay::recordDrawCall(size_t triangleCount, size_t vertexCount) {
        m_currentFrameDrawCalls++;
        m_currentFrameTriangles += triangleCount;
        m_currentFrameVertices += vertexCount;
    }

    void PerformanceOverlay::setEntityStats(size_t totalEntities, size_t activeEntities, size_t componentCount) {
        m_stats.entityCount = totalEntities;
        m_stats.activeEntities = activeEntities;
        m_stats.componentCount = componentCount;
    }

    void PerformanceOverlay::onWindowResize(int newWidth, int newHeight) {
        m_windowWidth = newWidth;
        m_windowHeight = newHeight;
    }

    bool PerformanceOverlay::initializeTextRendering() {
        // Create simple quad for background rendering
        float quadVertices[] = {
            // positions   // texCoords
            0.0f, 1.0f,   0.0f, 1.0f,
            1.0f, 0.0f,   1.0f, 0.0f,
            0.0f, 0.0f,   0.0f, 0.0f,
            
            0.0f, 1.0f,   0.0f, 1.0f,
            1.0f, 1.0f,   1.0f, 1.0f,
            1.0f, 0.0f,   1.0f, 0.0f
        };

        glGenVertexArrays(1, &m_quadVAO);
        glGenBuffers(1, &m_quadVBO);
        
        glBindVertexArray(m_quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Create simple shaders for text and quad rendering
        const char* vertexShaderSource = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoord;
            
            uniform mat4 projection;
            uniform vec2 position;
            uniform vec2 scale;
            
            out vec2 TexCoord;
            
            void main() {
                vec2 screenPos = (aPos * scale + position) / vec2(800, 600); // Normalized coordinates
                screenPos = screenPos * 2.0 - 1.0; // Convert to NDC
                screenPos.y = -screenPos.y; // Flip Y coordinate
                gl_Position = vec4(screenPos, 0.0, 1.0);
                TexCoord = aTexCoord;
            }
        )";

        const char* fragmentShaderSource = R"(
            #version 330 core
            out vec4 FragColor;
            
            uniform vec4 color;
            
            void main() {
                FragColor = color;
            }
        )";

        // Compile shaders (simplified - in a real implementation you'd have proper error checking)
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        m_quadShaderProgram = glCreateProgram();
        glAttachShader(m_quadShaderProgram, vertexShader);
        glAttachShader(m_quadShaderProgram, fragmentShader);
        glLinkProgram(m_quadShaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return true;
    }

    void PerformanceOverlay::updateFPSStats(float deltaTime) {
        // Update frame time samples
        m_frameTimes[m_frameTimeIndex] = deltaTime * 1000.0f; // Convert to ms
        m_frameTimeIndex = (m_frameTimeIndex + 1) % FRAME_TIME_SAMPLES;

        m_frameTimeAccumulator += deltaTime;
        m_frameCount++;

        // Calculate current FPS
        m_stats.currentFPS = 1.0f / deltaTime;
        m_stats.frameTime = deltaTime * 1000.0f;

        // Calculate average FPS every second
        if (m_frameTimeAccumulator >= 1.0f) {
            m_stats.averageFPS = m_frameCount / m_frameTimeAccumulator;
            m_stats.averageFrameTime = (m_frameTimeAccumulator / m_frameCount) * 1000.0f;
            
            // Reset accumulators
            m_frameTimeAccumulator = 0.0f;
            m_frameCount = 0;
        }

        // Calculate min/max frame times
        auto minMax = std::minmax_element(m_frameTimes.begin(), m_frameTimes.end());
        m_stats.minFrameTime = *minMax.first;
        m_stats.maxFrameTime = *minMax.second;
    }

    void PerformanceOverlay::updateSystemStats() {
        // Update memory usage
        m_stats.memoryUsage = getSystemMemoryUsage();
        m_stats.peakMemoryUsage = std::max(m_stats.peakMemoryUsage, m_stats.memoryUsage);

        // Update CPU usage
        m_stats.cpuUsage = getCPUUsage();
    }

    void PerformanceOverlay::updateGPUStats() {
        if (!m_gpuTimingSupported) {
            return;
        }

        // Query GPU timing
        GLuint64 gpuTime = 0;
        glGetQueryObjectui64v(m_gpuQueries[m_currentQuery], GL_QUERY_RESULT, &gpuTime);
        m_stats.gpuFrameTime = gpuTime / 1000000.0f; // Convert nanoseconds to milliseconds

        // Start timing for next frame
        m_currentQuery = 1 - m_currentQuery;
        glBeginQuery(GL_TIME_ELAPSED, m_gpuQueries[m_currentQuery]);

        // Get GPU memory info
        getGPUMemoryInfo(m_stats.gpuMemoryUsage, m_stats.totalGpuMemory);
    }

    void PerformanceOverlay::renderText(const std::string& text, float x, float y, const glm::vec4& color) {
        // This is a simplified text rendering implementation
        // In a real engine, you would use a proper font rendering library like FreeType
        
        // For now, we'll render a colored rectangle as a placeholder for each character
        float charWidth = 8.0f;
        float charHeight = 12.0f;
        
        for (size_t i = 0; i < text.length(); ++i) {
            renderBackground(x + i * charWidth, y, charWidth * 0.8f, charHeight, color);
        }
    }

    void PerformanceOverlay::renderBackground(float x, float y, float width, float height, const glm::vec4& color) {
        if (!m_quadShaderProgram || !m_quadVAO) {
            return;
        }

        glUseProgram(m_quadShaderProgram);
        
        // Set uniforms
        GLint positionLoc = glGetUniformLocation(m_quadShaderProgram, "position");
        GLint scaleLoc = glGetUniformLocation(m_quadShaderProgram, "scale");
        GLint colorLoc = glGetUniformLocation(m_quadShaderProgram, "color");
        
        if (positionLoc >= 0) {
            glUniform2f(positionLoc, x, y);
        }
        if (scaleLoc >= 0) {
            glUniform2f(scaleLoc, width, height);
        }
        if (colorLoc >= 0) {
            glUniform4f(colorLoc, color.r, color.g, color.b, color.a);
        }

        glBindVertexArray(m_quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        
        glUseProgram(0);
    }

    size_t PerformanceOverlay::getSystemMemoryUsage() {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize / (1024 * 1024); // Convert to MB
        }
        return 0;
#else
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            return usage.ru_maxrss / 1024; // Convert to MB (rusage returns KB on Linux)
        }
        return 0;
#endif
    }

    float PerformanceOverlay::getCPUUsage() {
        // This is a simplified CPU usage calculation
        // In a real implementation, you would track CPU time over intervals
        static auto lastTime = std::chrono::high_resolution_clock::now();
        static clock_t lastClock = clock();
        
        auto currentTime = std::chrono::high_resolution_clock::now();
        clock_t currentClock = clock();
        
        auto realTime = std::chrono::duration<double>(currentTime - lastTime).count();
        double cpuTime = static_cast<double>(currentClock - lastClock) / CLOCKS_PER_SEC;
        
        lastTime = currentTime;
        lastClock = currentClock;
        
        if (realTime > 0) {
            return static_cast<float>((cpuTime / realTime) * 100.0);
        }
        return 0.0f;
    }

    void PerformanceOverlay::getGPUMemoryInfo(size_t& used, size_t& total) {
        // Simplified GPU memory detection - return placeholder values
        total = 8192; // 8GB placeholder
        used = 2048;  // 2GB placeholder
        
        // Note: Advanced GPU memory queries disabled for compatibility
        // Real implementations would check NVIDIA/AMD specific extensions
    }

    void PerformanceOverlay::calculatePerformanceIndicators() {
        // Calculate frame time variance
        float meanFrameTime = 0.0f;
        for (float frameTime : m_frameTimes) {
            meanFrameTime += frameTime;
        }
        meanFrameTime /= FRAME_TIME_SAMPLES;

        float variance = 0.0f;
        for (float frameTime : m_frameTimes) {
            float diff = frameTime - meanFrameTime;
            variance += diff * diff;
        }
        m_stats.frameTimeVariance = variance / FRAME_TIME_SAMPLES;

        // Determine if performance is good (>= 30 FPS and low variance)
        m_stats.isPerformanceGood = (m_stats.currentFPS >= 30.0f) && (m_stats.frameTimeVariance < 100.0f);
    }

    void PerformanceOverlay::resetFrameStats() {
        // Copy current frame stats to accumulated stats
        m_stats.drawCalls = m_currentFrameDrawCalls;
        m_stats.triangles = m_currentFrameTriangles;
        m_stats.vertices = m_currentFrameVertices;

        // Reset current frame counters
        m_currentFrameDrawCalls = 0;
        m_currentFrameTriangles = 0;
        m_currentFrameVertices = 0;
    }

} // namespace IKore
