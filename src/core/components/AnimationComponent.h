#pragma once

#include "core/Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <assimp/scene.h>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <functional>
#include <set>

namespace IKore {

    // Maximum number of bones per vertex (commonly 4)
    constexpr int MAX_BONE_INFLUENCE = 4;
    constexpr int MAX_BONES = 100;

    // Bone information for vertices
    struct BoneInfo {
        int id = -1;
        glm::mat4 offset = glm::mat4(1.0f);
    };

    // Extended vertex structure with bone data
    struct AnimatedVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoords;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        
        // Bone data
        int boneIDs[MAX_BONE_INFLUENCE] = {-1, -1, -1, -1};
        float weights[MAX_BONE_INFLUENCE] = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    // Animation keyframe structures
    struct VectorKey {
        glm::vec3 value;
        float timeStamp;
    };

    struct QuatKey {
        glm::quat value;
        float timeStamp;
    };

    // Bone node in animation hierarchy
    struct Bone {
        std::string name;
        int id;
        glm::mat4 localTransform = glm::mat4(1.0f);
        std::vector<VectorKey> positions;
        std::vector<QuatKey> rotations;
        std::vector<VectorKey> scales;
        
        // Interpolation methods
        glm::mat4 interpolatePosition(float animationTime);
        glm::mat4 interpolateRotation(float animationTime);
        glm::mat4 interpolateScaling(float animationTime);
        
        int getPositionIndex(float animationTime);
        int getRotationIndex(float animationTime);
        int getScaleIndex(float animationTime);
        
        float getScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime);
        
        void update(float animationTime);
    };

    // Animation data
    struct Animation {
        std::string name;
        float duration;
        float ticksPerSecond;
        std::vector<Bone> bones;
        std::map<std::string, BoneInfo> boneInfoMap;
        
        Bone* findBone(const std::string& name);
    };

    // Animation state for blending
    struct AnimationState {
        Animation* animation = nullptr;
        float currentTime = 0.0f;
        float weight = 1.0f;
        float speed = 1.0f;
        bool looping = true;
        bool playing = false;
        bool finished = false;
    };

    // Animation transition data
    struct AnimationTransition {
        Animation* fromAnimation = nullptr;
        Animation* toAnimation = nullptr;
        float duration = 0.5f;
        float currentTime = 0.0f;
        bool active = false;
    };

    class AnimationComponent : public Component {
    public:
        AnimationComponent();
        ~AnimationComponent() override = default;

        // Component interface
        void update(float deltaTime);
        void initialize();
        void cleanup();
        std::unique_ptr<Component> clone() const;

        // Animation management
        void addAnimation(const std::string& name, std::unique_ptr<Animation> animation);
        void playAnimation(const std::string& name, bool loop = true, float speed = 1.0f);
        void stopAnimation();
        void pauseAnimation();
        void resumeAnimation();
        void setAnimationSpeed(float speed);
        void setAnimationTime(float time);
        
        // Animation blending and transitions
        void blendToAnimation(const std::string& name, float transitionDuration = 0.5f);
        void addAnimationLayer(const std::string& name, float weight = 1.0f);
        void setLayerWeight(const std::string& name, float weight);
        void removeAnimationLayer(const std::string& name);
        
        // Bone and transform access
        const std::vector<glm::mat4>& getBoneTransforms() const { return m_finalBoneMatrices; }
        glm::mat4 getBoneTransform(const std::string& boneName) const;
        glm::mat4 getBoneTransform(int boneIndex) const;
        void setBoneTransform(const std::string& boneName, const glm::mat4& transform);
        void setBoneTransform(int boneIndex, const glm::mat4& transform);
        
        // Animation queries
        bool isPlaying() const { return m_currentState.playing; }
        bool isPaused() const { return !m_currentState.playing && m_currentState.currentTime > 0.0f; }
        bool isFinished() const { return m_currentState.finished; }
        float getCurrentTime() const { return m_currentState.currentTime; }
        float getDuration() const;
        const std::string& getCurrentAnimationName() const { return m_currentAnimationName; }
        
        // Bone mapping and skeleton
        void setBoneInfoMap(const std::map<std::string, BoneInfo>& boneInfoMap);
        const std::map<std::string, BoneInfo>& getBoneInfoMap() const { return m_boneInfoMap; }
        int getBoneCount() const { return static_cast<int>(m_boneInfoMap.size()); }
        
        // Animation events and callbacks
        using AnimationEventCallback = std::function<void(const std::string&)>;
        void setAnimationFinishedCallback(AnimationEventCallback callback);
        void setAnimationStartedCallback(AnimationEventCallback callback);
        void addAnimationEvent(float time, const std::string& eventName);
        
        // Root motion
        void enableRootMotion(bool enable) { m_rootMotionEnabled = enable; }
        void setRootMotionEnabled(bool enable) { m_rootMotionEnabled = enable; } // Alias
        bool isRootMotionEnabled() const { return m_rootMotionEnabled; }
        glm::mat4 getRootMotionDelta() const { return m_rootMotionDelta; }

    private:
        // Animation data
        std::map<std::string, std::unique_ptr<Animation>> m_animations;
        std::string m_currentAnimationName;
        AnimationState m_currentState;
        AnimationTransition m_transition;
        
        // Multiple animation layers for blending
        std::map<std::string, AnimationState> m_animationLayers;
        
        // Bone and skeleton data
        std::map<std::string, BoneInfo> m_boneInfoMap;
        std::vector<glm::mat4> m_finalBoneMatrices;
        glm::mat4 m_globalInverseTransform = glm::mat4(1.0f);
        
        // Root motion
        bool m_rootMotionEnabled = false;
        glm::mat4 m_rootMotionDelta = glm::mat4(1.0f);
        glm::mat4 m_previousRootTransform = glm::mat4(1.0f);
        
        // Animation events
        std::map<float, std::vector<std::string>> m_animationEvents;
        std::set<std::string> m_triggeredEvents;
        AnimationEventCallback m_onAnimationFinished;
        AnimationEventCallback m_onAnimationStarted;
        
        // Internal methods
        void updateAnimation(float deltaTime);
        void updateTransition(float deltaTime);
        void updateAnimationLayer(AnimationState& state, float deltaTime);
        void calculateBoneTransform(Animation* animation, float animationTime, 
                                  const aiNode* node, const glm::mat4& parentTransform);
        void blendBoneTransforms(const std::vector<glm::mat4>& transforms1, 
                               const std::vector<glm::mat4>& transforms2, 
                               float weight, std::vector<glm::mat4>& result);
        void processAnimationEvents(float previousTime, float currentTime);
        void updateRootMotion(float deltaTime);
        
        // Utility methods
        glm::mat4 assimpToGlmMatrix(const aiMatrix4x4& from);
        glm::vec3 assimpToGlmVec3(const aiVector3D& vec);
        glm::quat assimpToGlmQuat(const aiQuaternion& quat);
    };

} // namespace IKore
