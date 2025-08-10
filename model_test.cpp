#include <iostream>
#include "Model.h"
#include "Logger.h"

int main() {
    // Initialize logging system
    IKore::Logger::getInstance().initialize();
    std::cout << "Starting model loading test..." << std::endl;

    // Test model loading
    IKore::Model testModel;
    std::cout << "Created model object" << std::endl;
    
    bool loaded = testModel.loadFromFile("assets/models/cube.obj");
    std::cout << "Model loading result: " << (loaded ? "SUCCESS" : "FAILED") << std::endl;
    
    if (loaded) {
        std::cout << "Number of meshes: " << testModel.getMeshes().size() << std::endl;
        
        for (size_t i = 0; i < testModel.getMeshes().size(); ++i) {
            const auto& mesh = testModel.getMeshes()[i];
            std::cout << "Mesh " << i << " - Vertices: " << mesh->getVertices().size() 
                      << ", Indices: " << mesh->getIndices().size() << std::endl;
        }
    }

    return 0;
}
