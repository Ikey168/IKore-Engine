#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

class TestLogger {
private:
    std::ofstream m_logFile;
    std::mutex m_mutex;
    bool m_initialized = false;

public:
    static TestLogger& getInstance() {
        std::cout << "Getting TestLogger instance..." << std::endl;
        static TestLogger instance;
        std::cout << "TestLogger instance obtained." << std::endl;
        return instance;
    }
    
    void initialize(const std::string& logFilePath = "logs/engine.log") {
        std::cout << "Starting TestLogger::initialize..." << std::endl;
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << "Mutex locked." << std::endl;
        
        if (m_initialized) {
            std::cout << "Already initialized, returning." << std::endl;
            return;
        }
        
        // Create logs directory if it doesn't exist
        std::filesystem::path filePath(logFilePath);
        std::filesystem::path directory = filePath.parent_path();
        
        if (!directory.empty() && !std::filesystem::exists(directory)) {
            std::filesystem::create_directories(directory);
        }
        
        std::cout << "Opening log file..." << std::endl;
        // Open log file
        m_logFile.open(logFilePath, std::ios::out | std::ios::app);
        if (!m_logFile.is_open()) {
            std::cerr << "Failed to open log file: " << logFilePath << std::endl;
            return;
        }
        
        std::cout << "Setting initialized flag..." << std::endl;
        m_initialized = true;
        
        std::cout << "Calling log method..." << std::endl;
        // Log initialization message
        log("TestLogger initialized");
    }
    
    void log(const std::string& message) {
        std::cout << "Starting TestLogger::log..." << std::endl;
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << "Log mutex locked." << std::endl;
        
        if (!m_initialized) {
            // If not initialized, just print to console
            std::cout << "[UNINIT] " << message << std::endl;
            return;
        }
        
        std::string logMessage = "[INIT] " + message;
        
        // Output to console
        std::cout << logMessage << std::endl;
        
        // Output to file
        if (m_logFile.is_open()) {
            m_logFile << logMessage << std::endl;
            m_logFile.flush(); // Ensure immediate write
        }
        std::cout << "Log method completed." << std::endl;
    }
};

int main() {
    std::cout << "Starting TestLogger test..." << std::endl;
    
    // Initialize Logger
    std::cout << "About to call getInstance..." << std::endl;
    TestLogger::getInstance().initialize("logs/test.log");
    std::cout << "TestLogger initialized successfully" << std::endl;
    
    // Test logging
    TestLogger::getInstance().log("Test message 1");
    std::cout << "First log message sent" << std::endl;
    
    TestLogger::getInstance().log("Test message 2");
    std::cout << "Second log message sent" << std::endl;
    
    std::cout << "Test completed successfully" << std::endl;
    
    return 0;
}
