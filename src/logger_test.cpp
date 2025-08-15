#include "Logger.h"
#include <iostream>

using namespace IKore;

int main() {
    std::cout << "Testing Logger initialization..." << std::endl;
    
    // Initialize Logger
    Logger::getInstance().initialize("logs/test.log");
    std::cout << "Logger initialized successfully" << std::endl;
    
    // Test logging
    Logger::getInstance().log(LogLevel::INFO, "Test message 1");
    std::cout << "First log message sent" << std::endl;
    
    Logger::getInstance().log(LogLevel::INFO, "Test message 2");
    std::cout << "Second log message sent" << std::endl;
    
    // Shutdown
    Logger::getInstance().shutdown();
    std::cout << "Logger shutdown successfully" << std::endl;
    
    return 0;
}
