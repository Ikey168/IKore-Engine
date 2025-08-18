#pragma once

#include <memory>
#include <string>
#include <vector>
#include <assimp/scene.h>
#include "components/AnimationComponent.h"

namespace IKore {

/**
 * Utility class for loading animations from Assimp scenes
 */
class AnimationLoader {
public:
    /**
     * Load all animations from an Assimp scene
     * @param scene The Assimp scene containing animations
     * @param boneInfoMap Bone mapping from the model
     * @return Vector of loaded animations
     */
    static std::vector<std::unique_ptr<Animation>> loadAnimations(
        const aiScene* scene, 
        const std::map<std::string, BoneInfo>& boneInfoMap
    );
    
    /**
     * Load a specific animation by index
     * @param scene The Assimp scene containing animations  
     * @param animationIndex Index of the animation to load
     * @param boneInfoMap Bone mapping from the model
     * @return Loaded animation or nullptr if failed
     */
    static std::unique_ptr<Animation> loadAnimation(
        const aiScene* scene, 
        unsigned int animationIndex,
        const std::map<std::string, BoneInfo>& boneInfoMap
    );
    
    /**
     * Load a specific animation by name
     * @param scene The Assimp scene containing animations
     * @param animationName Name of the animation to load
     * @param boneInfoMap Bone mapping from the model
     * @return Loaded animation or nullptr if failed
     */
    static std::unique_ptr<Animation> loadAnimationByName(
        const aiScene* scene,
        const std::string& animationName,
        const std::map<std::string, BoneInfo>& boneInfoMap
    );

private:
    /**
     * Read bone position keyframes
     */
    static void readPositionKeyframes(aiNodeAnim* channel, Bone& bone);
    
    /**
     * Read bone rotation keyframes
     */
    static void readRotationKeyframes(aiNodeAnim* channel, Bone& bone);
    
    /**
     * Read bone scale keyframes
     */
    static void readScaleKeyframes(aiNodeAnim* channel, Bone& bone);
    
    /**
     * Convert Assimp time to seconds
     */
    static float convertTime(double assimpTime, double ticksPerSecond);
    
    /**
     * Convert Assimp vector to GLM vector
     */
    static glm::vec3 assimpToGlmVec3(const aiVector3D& vec);
    
    /**
     * Convert Assimp quaternion to GLM quaternion
     */
    static glm::quat assimpToGlmQuat(const aiQuaternion& quat);
};

} // namespace IKore
