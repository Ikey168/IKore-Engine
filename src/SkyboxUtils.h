// Simple test function to create colored skybox textures programmatically
// This will be used for testing when actual image files are not available

#include <vector>
#include <cstdint>

namespace IKore {
    namespace SkyboxUtils {
        
        // Generate a simple gradient skybox programmatically
        std::vector<std::vector<uint8_t>> generateTestSkybox(int size = 512) {
            std::vector<std::vector<uint8_t>> faces(6);
            
            // Colors for each face: right, left, top, bottom, front, back
            std::vector<std::vector<uint8_t>> faceColors = {
                {255, 100, 100}, // right - red
                {100, 255, 100}, // left - green  
                {100, 100, 255}, // top - blue
                {255, 255, 100}, // bottom - yellow
                {255, 100, 255}, // front - magenta
                {100, 255, 255}  // back - cyan
            };
            
            for (int face = 0; face < 6; face++) {
                faces[face].resize(size * size * 3);
                
                for (int y = 0; y < size; y++) {
                    for (int x = 0; x < size; x++) {
                        int index = (y * size + x) * 3;
                        
                        // Create gradient effect
                        float gradientX = static_cast<float>(x) / size;
                        float gradientY = static_cast<float>(y) / size;
                        float intensity = 0.5f + 0.5f * (gradientX + gradientY) * 0.5f;
                        
                        faces[face][index + 0] = static_cast<uint8_t>(faceColors[face][0] * intensity);
                        faces[face][index + 1] = static_cast<uint8_t>(faceColors[face][1] * intensity);
                        faces[face][index + 2] = static_cast<uint8_t>(faceColors[face][2] * intensity);
                    }
                }
            }
            
            return faces;
        }
    }
}
