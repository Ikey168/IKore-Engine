#include "Logger.h"
#include <iostream>

int main() {
    try {
        std::cout << "Initializing Logger..." << std::endl;
        IKore::Logger::getInstance().initialize();
        std::cout << "Logger initialized successfully!" << std::endl;
        
        std::cout << "Testing log messages..." << std::endl;
        IKore::Logger::getInstance().log(IKore::LogLevel::INFO, "Test message");
        std::cout << "Log test completed!" << std::endl;
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown exception!" << std::endl;
        return 1;
    }
}
