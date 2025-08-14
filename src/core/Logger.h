#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace IKore {

enum class LogLevel {
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& getInstance();
    
    void initialize(const std::string& logFilePath = "logs/engine.log");
    void shutdown();
    
    void log(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

private:
    Logger() = default;
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string getCurrentTimestamp();
    std::string logLevelToString(LogLevel level);
    
    std::ofstream m_logFile;
    std::mutex m_mutex;
    bool m_initialized = false;
};

// Convenience macros
#define LOG_INFO(msg) IKore::Logger::getInstance().info(msg)
#define LOG_WARNING(msg) IKore::Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) IKore::Logger::getInstance().error(msg)

}
