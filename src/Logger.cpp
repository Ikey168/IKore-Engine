#include "Logger.h"
#include <filesystem>

namespace IKore {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::initialize(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        return;
    }
    
    // Create logs directory if it doesn't exist
    std::filesystem::path filePath(logFilePath);
    std::filesystem::path directory = filePath.parent_path();
    
    if (!directory.empty() && !std::filesystem::exists(directory)) {
        std::filesystem::create_directories(directory);
    }
    
    // Open log file
    m_logFile.open(logFilePath, std::ios::out | std::ios::app);
    if (!m_logFile.is_open()) {
        std::cerr << "Failed to open log file: " << logFilePath << std::endl;
        return;
    }
    
    m_initialized = true;
    
    // Log initialization message
    log(LogLevel::INFO, "Logger initialized - IKore Engine");
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        log(LogLevel::INFO, "Logger shutting down");
        m_logFile.close();
        m_initialized = false;
    }
}

Logger::~Logger() {
    shutdown();
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        // If not initialized, just print to console
        std::cout << "[" << logLevelToString(level) << "] " << message << std::endl;
        return;
    }
    
    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = logLevelToString(level);
    std::string logMessage = "[" + timestamp + "] [" + levelStr + "] " + message;
    
    // Output to console
    std::cout << logMessage << std::endl;
    
    // Output to file
    if (m_logFile.is_open()) {
        m_logFile << logMessage << std::endl;
        m_logFile.flush(); // Ensure immediate write
    }
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string Logger::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

}
