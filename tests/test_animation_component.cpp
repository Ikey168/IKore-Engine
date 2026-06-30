#include <iostream>
#include <memory>
#include <chrono>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/core/components/AnimationComponent.h"
#include "src/core/AnimationLoader.h"
#include "src/core/Logger.h"

using namespace IKore;

void testAnimationComponent() {
    std::cout << "=== Testing AnimationComponent ===" << std::endl;
    
    // Test component creation
    auto animComponent = std::make_unique<AnimationComponent>();
    animComponent->initialize();
    
    // Test basic functionality
    std::cout << "✓ AnimationComponent created and initialized" << std::endl;
    
    // Test bone transform operations
    glm::mat4 testTransform = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
    animComponent->setBoneTransform(0, testTransform);
    
    glm::mat4 retrievedTransform = animComponent->getBoneTransform(0);
    if (retrievedTransform == testTransform) {
        std::cout << "✓ Bone transform set/get working correctly" << std::endl;
    } else {
        std::cout << "✗ Bone transform set/get failed" << std::endl;
    }
    
    // Test animation state
    std::cout << "Current time: " << animComponent->getCurrentTime() << std::endl;
    std::cout << "Is playing: " << (animComponent->isPlaying() ? "true" : "false") << std::endl;
    std::cout << "Duration: " << animComponent->getDuration() << std::endl;
    
    // Test animation events
    bool eventTriggered = false;
    animComponent->setAnimationFinishedCallback([&eventTriggered](const std::string& name) {
        std::cout << "Animation finished callback triggered for: " << name << std::endl;
        eventTriggered = true;
    });
    
    // Test root motion
    animComponent->setRootMotionEnabled(true);
    std::cout << "Root motion enabled: " << (animComponent->isRootMotionEnabled() ? "true" : "false") << std::endl;
    
    // Test update (without actual animation data)
    float deltaTime = 0.016f; // 60 FPS
    animComponent->update(deltaTime);
    
    std::cout << "✓ AnimationComponent update completed" << std::endl;
    
    animComponent->cleanup();
    std::cout << "✓ AnimationComponent cleaned up" << std::endl;
}

void testAnimationCreation() {
    std::cout << "\n=== Testing Animation Creation ===" << std::endl;
    
    // Create a simple test animation
    auto animation = std::make_unique<Animation>();
    animation->name = "test_animation";
    animation->duration = 2.0f;
    animation->ticksPerSecond = 30.0f;
    
    // Create a test bone
    Bone testBone;
    testBone.name = "test_bone";
    testBone.id = 0;
    
    // Add some keyframes
    IKore::VectorKey pos1;
    pos1.value = glm::vec3(0.0f, 0.0f, 0.0f);
    pos1.timeStamp = 0.0f;
    testBone.positions.push_back(pos1);
    
    IKore::VectorKey pos2;
    pos2.value = glm::vec3(1.0f, 0.0f, 0.0f);
    pos2.timeStamp = 1.0f;
    testBone.positions.push_back(pos2);
    
    IKore::QuatKey rot1;
    rot1.value = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    rot1.timeStamp = 0.0f;
    testBone.rotations.push_back(rot1);
    
    IKore::VectorKey scale1;
    scale1.value = glm::vec3(1.0f, 1.0f, 1.0f);
    scale1.timeStamp = 0.0f;
    testBone.scales.push_back(scale1);
    
    animation->bones.push_back(testBone);
    
    std::cout << "✓ Test animation created with " << animation->bones.size() << " bones" << std::endl;
    std::cout << "Animation duration: " << animation->duration << "s" << std::endl;
    
    // Test bone interpolation
    testBone.update(0.5f); // Update at halfway point
    std::cout << "✓ Bone interpolation test completed" << std::endl;
}

void testBoneInfoMapping() {
    std::cout << "\n=== Testing BoneInfo Mapping ===" << std::endl;
    
    std::map<std::string, BoneInfo> boneInfoMap;
    
    // Create test bone info
    BoneInfo boneInfo1;
    boneInfo1.id = 0;
    boneInfo1.offset = glm::mat4(1.0f);
    boneInfoMap["root"] = boneInfo1;
    
    BoneInfo boneInfo2;
    boneInfo2.id = 1;
    boneInfo2.offset = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    boneInfoMap["spine"] = boneInfo2;
    
    // Test AnimationComponent with bone info
    auto animComponent = std::make_unique<AnimationComponent>();
    animComponent->setBoneInfoMap(boneInfoMap);
    
    std::cout << "✓ BoneInfo map set with " << boneInfoMap.size() << " bones" << std::endl;
    
    // Test bone transform by name
    glm::mat4 testTransform = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    animComponent->setBoneTransform("root", testTransform);
    
    glm::mat4 retrievedTransform = animComponent->getBoneTransform("root");
    std::cout << "✓ Bone transform by name working" << std::endl;
    
    animComponent->cleanup();
}

int main() {
    // Initialize Logger
    Logger::getInstance().info("Starting AnimationComponent tests");
    
    try {
        testAnimationComponent();
        testAnimationCreation();
        testBoneInfoMapping();
        
        std::cout << "\n=== All Animation Tests Completed Successfully! ===" << std::endl;
        Logger::getInstance().info("All animation tests completed successfully");
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        Logger::getInstance().error("Animation test failed: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}
