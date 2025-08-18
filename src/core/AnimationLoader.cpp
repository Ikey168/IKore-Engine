#include "AnimationLoader.h"
#include "Logger.h"
#include <algorithm>

namespace IKore {

std::vector<std::unique_ptr<Animation>> AnimationLoader::loadAnimations(
    const aiScene* scene, 
    const std::map<std::string, BoneInfo>& boneInfoMap) {
    
    std::vector<std::unique_ptr<Animation>> animations;
    
    if (!scene || scene->mNumAnimations == 0) {
        Logger::getInstance().warning("No animations found in scene");
        return animations;
    }
    
    animations.reserve(scene->mNumAnimations);
    
    for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
        auto animation = loadAnimation(scene, i, boneInfoMap);
        if (animation) {
            animations.push_back(std::move(animation));
        }
    }
    
    Logger::getInstance().info("Loaded " + std::to_string(animations.size()) + " animations");
    return animations;
}

std::unique_ptr<Animation> AnimationLoader::loadAnimation(
    const aiScene* scene, 
    unsigned int animationIndex,
    const std::map<std::string, BoneInfo>& boneInfoMap) {
    
    if (!scene || animationIndex >= scene->mNumAnimations) {
        Logger::getInstance().error("Invalid animation index: " + std::to_string(animationIndex));
        return nullptr;
    }
    
    aiAnimation* assimpAnimation = scene->mAnimations[animationIndex];
    auto animation = std::make_unique<Animation>();
    
    animation->name = assimpAnimation->mName.C_Str();
    animation->duration = convertTime(assimpAnimation->mDuration, assimpAnimation->mTicksPerSecond);
    animation->ticksPerSecond = assimpAnimation->mTicksPerSecond;
    
    // Process all bone channels
    for (unsigned int i = 0; i < assimpAnimation->mNumChannels; i++) {
        aiNodeAnim* channel = assimpAnimation->mChannels[i];
        std::string boneName = channel->mNodeName.C_Str();
        
        // Check if this bone exists in our bone info map
        if (boneInfoMap.find(boneName) == boneInfoMap.end()) {
            Logger::getInstance().warning("Bone '" + boneName + "' not found in bone info map");
            continue;
        }
        
        Bone bone;
        bone.name = boneName;
        bone.id = boneInfoMap.at(boneName).id;
        
        // Read keyframe data
        readPositionKeyframes(channel, bone);
        readRotationKeyframes(channel, bone);
        readScaleKeyframes(channel, bone);
        
        animation->bones.push_back(bone);
    }
    
    Logger::getInstance().info("Loaded animation: " + animation->name + 
                              " (duration: " + std::to_string(animation->duration) + "s, " +
                              std::to_string(animation->bones.size()) + " bones)");
    
    return animation;
}

std::unique_ptr<Animation> AnimationLoader::loadAnimationByName(
    const aiScene* scene,
    const std::string& animationName,
    const std::map<std::string, BoneInfo>& boneInfoMap) {
    
    if (!scene) {
        Logger::getInstance().error("Invalid scene provided");
        return nullptr;
    }
    
    // Find animation by name
    for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
        if (scene->mAnimations[i]->mName.C_Str() == animationName) {
            return loadAnimation(scene, i, boneInfoMap);
        }
    }
    
    Logger::getInstance().warning("Animation not found: " + animationName);
    return nullptr;
}

void AnimationLoader::readPositionKeyframes(aiNodeAnim* channel, Bone& bone) {
    bone.positions.reserve(channel->mNumPositionKeys);
    
    for (unsigned int i = 0; i < channel->mNumPositionKeys; i++) {
        aiVectorKey& key = channel->mPositionKeys[i];
        VectorKey keyPosition;
        keyPosition.value = assimpToGlmVec3(key.mValue);
        keyPosition.timeStamp = convertTime(key.mTime, 1.0); // Normalized time
        bone.positions.push_back(keyPosition);
    }
}

void AnimationLoader::readRotationKeyframes(aiNodeAnim* channel, Bone& bone) {
    bone.rotations.reserve(channel->mNumRotationKeys);
    
    for (unsigned int i = 0; i < channel->mNumRotationKeys; i++) {
        aiQuatKey& key = channel->mRotationKeys[i];
        QuatKey keyRotation;
        keyRotation.value = assimpToGlmQuat(key.mValue);
        keyRotation.timeStamp = convertTime(key.mTime, 1.0); // Normalized time
        bone.rotations.push_back(keyRotation);
    }
}

void AnimationLoader::readScaleKeyframes(aiNodeAnim* channel, Bone& bone) {
    bone.scales.reserve(channel->mNumScalingKeys);
    
    for (unsigned int i = 0; i < channel->mNumScalingKeys; i++) {
        aiVectorKey& key = channel->mScalingKeys[i];
        VectorKey keyScale;
        keyScale.value = assimpToGlmVec3(key.mValue);
        keyScale.timeStamp = convertTime(key.mTime, 1.0); // Normalized time
        bone.scales.push_back(keyScale);
    }
}

float AnimationLoader::convertTime(double assimpTime, double ticksPerSecond) {
    // Convert from Assimp time units to seconds
    if (ticksPerSecond <= 0.0) {
        ticksPerSecond = 25.0; // Default fallback
    }
    return static_cast<float>(assimpTime / ticksPerSecond);
}

glm::vec3 AnimationLoader::assimpToGlmVec3(const aiVector3D& vec) {
    return glm::vec3(vec.x, vec.y, vec.z);
}

glm::quat AnimationLoader::assimpToGlmQuat(const aiQuaternion& quat) {
    return glm::quat(quat.w, quat.x, quat.y, quat.z);
}

} // namespace IKore
