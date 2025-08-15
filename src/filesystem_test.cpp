#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    std::cout << "Starting filesystem test..." << std::endl;
    
    std::string logFilePath = "logs/test.log";
    
    std::cout << "1. Creating filesystem path..." << std::endl;
    std::filesystem::path filePath(logFilePath);
    
    std::cout << "2. Getting parent path..." << std::endl;
    std::filesystem::path directory = filePath.parent_path();
    
    std::cout << "3. Directory path: " << directory << std::endl;
    std::cout << "4. Checking if directory is empty..." << std::endl;
    
    if (!directory.empty()) {
        std::cout << "5. Directory not empty, checking if exists..." << std::endl;
        
        if (!std::filesystem::exists(directory)) {
            std::cout << "6. Directory doesn't exist, creating..." << std::endl;
            std::filesystem::create_directories(directory);
            std::cout << "7. Directory created." << std::endl;
        } else {
            std::cout << "6. Directory already exists." << std::endl;
        }
    }
    
    std::cout << "8. Opening file..." << std::endl;
    std::ofstream logFile;
    logFile.open(logFilePath, std::ios::out | std::ios::app);
    
    if (!logFile.is_open()) {
        std::cout << "Failed to open log file!" << std::endl;
        return 1;
    }
    
    std::cout << "9. File opened successfully." << std::endl;
    logFile.close();
    
    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}
