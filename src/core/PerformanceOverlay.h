#pragma once

#include <memory>
#include <chrono>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>

namespace IKore {

    /**
     * @brief System performance statistics
     */
    struct SystemPerformanceStats {
        // FPS and frame timing
        float currentFPS = 0.0f;
        float averageFPS = 0.0f;
        float frameTime = 0.0f;          // Current frame time in ms
        float averageFrameTime = 0.0f;   // Average frame time in ms
        float minFrameTime = 0.0f;       // Minimum frame time in ms
        float maxFrameTime = 0.0f;       // Maximum frame time in ms
        
        // CPU statistics
        float cpuUsage = 0.0f;           // CPU usage percentage
        size_t memoryUsage = 0;          // Memory usage in MB
        size_t peakMemoryUsage = 0;      // Peak memory usage in MB
        
        // GPU statistics (estimates)
        float gpuFrameTime = 0.0f;       // GPU frame time in ms
        size_t gpuMemoryUsage = 0;       // GPU memory usage in MB
        size_t totalGpuMemory = 0;       // Total GPU memory in MB
        
        // Rendering statistics
        size_t drawCalls = 0;            // Number of draw calls per frame
        size_t triangles = 0;            // Number of triangles rendered
        size_t vertices = 0;             // Number of vertices processed
        
        // Engine-specific statistics
        size_t entityCount = 0;          // Total entities
        size_t activeEntities = 0;       // Active entities
        size_t componentCount = 0;       // Total components
        
        // Performance indicators
        float frameTimeVariance = 0.0f;  // Frame time consistency
        bool isPerformanceGood = true;   // Overall performance indicator
    };

    /**
     * @brief Performance monitoring overlay configuration
     */
    struct OverlayConfig {
        bool enabled = false;            // Whether overlay is visible
        glm::vec2 position = {10.0f, 10.0f}; // Screen position in pixels
        glm::vec4 backgroundColor = {0.0f, 0.0f, 0.0f, 0.7f}; // Background color
        glm::vec4 textColor = {1.0f, 1.0f, 1.0f, 1.0f};       // Text color
        glm::vec4 goodPerformanceColor = {0.0f, 1.0f, 0.0f, 1.0f}; // Green
        glm::vec4 badPerformanceColor = {1.0f, 0.0f, 0.0f, 1.0f};  // Red
        float fontSize = 14.0f;          // Font size
        float lineSpacing = 16.0f;       // Line spacing
        float padding = 8.0f;            // Padding around text
        float width = 300.0f;            // Overlay width
        bool showAdvancedStats = false;  // Show detailed statistics
        bool showGpuStats = true;        // Show GPU statistics
        bool showMemoryStats = true;     // Show memory statistics
        bool showRenderStats = true;     // Show rendering statistics
    };

    /**
     * @brief FPS Counter and Performance Statistics Overlay
     * 
     * This system provides real-time performance monitoring with minimal
     * overhead. It displays FPS, frame times, CPU/GPU usage, memory statistics,
     * and rendering metrics in a toggleable overlay.
     */
    class PerformanceOverlay {
    public:
        /**
         * @brief Get the singleton instance
         */
        static PerformanceOverlay& getInstance();

        /**
         * @brief Initialize the performance overlay
         * @param windowWidth Window width in pixels
         * @param windowHeight Window height in pixels
         * @return True if initialization succeeded
         */
        bool initialize(int windowWidth, int windowHeight);

        /**
         * @brief Shutdown the performance overlay
         */
        void shutdown();

        /**
         * @brief Update performance statistics
         * @param deltaTime Frame time in seconds
         */
        void update(float deltaTime);

        /**
         * @brief Render the performance overlay
         */
        void render();

        /**
         * @brief Toggle overlay visibility
         */
        void toggle();

        /**
         * @brief Show the overlay
         */
        void show();

        /**
         * @brief Hide the overlay
         */
        void hide();

        /**
         * @brief Check if overlay is visible
         */
        bool isVisible() const { return m_config.enabled; }

        /**
         * @brief Set overlay position
         * @param x X position in pixels
         * @param y Y position in pixels
         */
        void setPosition(float x, float y);

        /**
         * @brief Set overlay colors
         * @param background Background color
         * @param text Text color
         */
        void setColors(const glm::vec4& background, const glm::vec4& text);

        /**
         * @brief Enable/disable advanced statistics
         */
        void setShowAdvancedStats(bool show) { m_config.showAdvancedStats = show; }

        /**
         * @brief Get current performance statistics
         */
        const SystemPerformanceStats& getStats() const { return m_stats; }

        /**
         * @brief Get overlay configuration
         */
        OverlayConfig& getConfig() { return m_config; }

        /**
         * @brief Record a draw call for statistics
         * @param triangleCount Number of triangles in the draw call
         * @param vertexCount Number of vertices in the draw call
         */
        void recordDrawCall(size_t triangleCount = 0, size_t vertexCount = 0);

        /**
         * @brief Set entity statistics
         * @param totalEntities Total number of entities
         * @param activeEntities Number of active entities
         * @param componentCount Total number of components
         */
        void setEntityStats(size_t totalEntities, size_t activeEntities, size_t componentCount);

        /**
         * @brief Handle window resize
         * @param newWidth New window width
         * @param newHeight New window height
         */
        void onWindowResize(int newWidth, int newHeight);

    private:
        PerformanceOverlay() = default;
        ~PerformanceOverlay() = default;
        PerformanceOverlay(const PerformanceOverlay&) = delete;
        PerformanceOverlay& operator=(const PerformanceOverlay&) = delete;

        /**
         * @brief Initialize text rendering system
         */
        bool initializeTextRendering();

        /**
         * @brief Update FPS calculations
         * @param deltaTime Frame time in seconds
         */
        void updateFPSStats(float deltaTime);

        /**
         * @brief Update system resource statistics
         */
        void updateSystemStats();

        /**
         * @brief Update GPU statistics
         */
        void updateGPUStats();

        /**
         * @brief Render text to screen
         * @param text Text to render
         * @param x X position in pixels
         * @param y Y position in pixels
         * @param color Text color
         */
        void renderText(const std::string& text, float x, float y, const glm::vec4& color);

        /**
         * @brief Render background quad
         * @param x X position in pixels
         * @param y Y position in pixels
         * @param width Width in pixels
         * @param height Height in pixels
         * @param color Background color
         */
        void renderBackground(float x, float y, float width, float height, const glm::vec4& color);

        /**
         * @brief Get system memory usage in MB
         */
        size_t getSystemMemoryUsage();

        /**
         * @brief Get CPU usage percentage
         */
        float getCPUUsage();

        /**
         * @brief Get GPU memory information
         */
        void getGPUMemoryInfo(size_t& used, size_t& total);

        /**
         * @brief Calculate performance indicators
         */
        void calculatePerformanceIndicators();

        /**
         * @brief Reset frame statistics
         */
        void resetFrameStats();

    private:
        // Configuration
        OverlayConfig m_config;
        
        // Statistics
        SystemPerformanceStats m_stats;
        
        // FPS calculation
        std::vector<float> m_frameTimes;
        size_t m_frameTimeIndex = 0;
        static constexpr size_t FRAME_TIME_SAMPLES = 60;
        float m_frameTimeAccumulator = 0.0f;
        size_t m_frameCount = 0;
        
        // System monitoring
        std::chrono::steady_clock::time_point m_lastStatsUpdate;
        static constexpr float STATS_UPDATE_INTERVAL = 0.5f; // Update every 500ms
        
        // OpenGL objects for rendering
        GLuint m_textShaderProgram = 0;
        GLuint m_quadShaderProgram = 0;
        GLuint m_quadVAO = 0;
        GLuint m_quadVBO = 0;
        GLuint m_fontTexture = 0;
        
        // Window dimensions
        int m_windowWidth = 0;
        int m_windowHeight = 0;
        
        // Frame statistics for current frame
        size_t m_currentFrameDrawCalls = 0;
        size_t m_currentFrameTriangles = 0;
        size_t m_currentFrameVertices = 0;
        
        // Initialization state
        bool m_initialized = false;
        
        // GPU timing
        GLuint m_gpuQueries[2] = {0, 0};
        bool m_gpuTimingSupported = false;
        size_t m_currentQuery = 0;
    };

    // Global function for easy access
    PerformanceOverlay& getPerformanceOverlay();

} // namespace IKore
