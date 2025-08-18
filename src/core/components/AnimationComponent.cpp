#include "AnimationComponent.h"
#include "core/Logger.h"
#include "TransformComponent.h"
#include <algorithm>
#include <cassert>

namespace IKore {

    // Bone implementation
    glm::mat4 Bone::interpolatePosition(float animationTime) {
        if (positions.size() == 1) {
            return glm::translate(glm::mat4(1.0f), positions[0].value);
        }

        int p0Index = getPositionIndex(animationTime);
        int p1Index = p0Index + 1;
        float scaleFactor = getScaleFactor(positions[p0Index].timeStamp, 
                                         positions[p1Index].timeStamp, animationTime);
        glm::vec3 finalPosition = glm::mix(positions[p0Index].value, 
                                         positions[p1Index].value, scaleFactor);
        return glm::translate(glm::mat4(1.0f), finalPosition);
    }

    glm::mat4 Bone::interpolateRotation(float animationTime) {
        if (rotations.size() == 1) {
            return glm::mat4_cast(glm::normalize(rotations[0].value));
        }

        int p0Index = getRotationIndex(animationTime);
        int p1Index = p0Index + 1;
        float scaleFactor = getScaleFactor(rotations[p0Index].timeStamp, 
                                         rotations[p1Index].timeStamp, animationTime);
        glm::quat finalRotation = glm::slerp(rotations[p0Index].value, 
                                           rotations[p1Index].value, scaleFactor);
        return glm::mat4_cast(glm::normalize(finalRotation));
    }

    glm::mat4 Bone::interpolateScaling(float animationTime) {
        if (scales.size() == 1) {
            return glm::scale(glm::mat4(1.0f), scales[0].value);
        }

        int p0Index = getScaleIndex(animationTime);
        int p1Index = p0Index + 1;
        float scaleFactor = getScaleFactor(scales[p0Index].timeStamp, 
                                         scales[p1Index].timeStamp, animationTime);
        glm::vec3 finalScale = glm::mix(scales[p0Index].value, 
                                      scales[p1Index].value, scaleFactor);
        return glm::scale(glm::mat4(1.0f), finalScale);
    }

    int Bone::getPositionIndex(float animationTime) {
        for (int index = 0; index < positions.size() - 1; ++index) {
            if (animationTime < positions[index + 1].timeStamp) {
                return index;
            }
        }
        return 0;
    }

    int Bone::getRotationIndex(float animationTime) {
        for (int index = 0; index < rotations.size() - 1; ++index) {
            if (animationTime < rotations[index + 1].timeStamp) {
                return index;
            }
        }
        return 0;
    }

    int Bone::getScaleIndex(float animationTime) {
        for (int index = 0; index < scales.size() - 1; ++index) {
            if (animationTime < scales[index + 1].timeStamp) {
                return index;
            }
        }
        return 0;
    }

    float Bone::getScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime) {
        float scaleFactor = 0.0f;
        float midWayLength = animationTime - lastTimeStamp;
        float framesDiff = nextTimeStamp - lastTimeStamp;
        scaleFactor = midWayLength / framesDiff;
        return scaleFactor;
    }

    void Bone::update(float animationTime) {
        glm::mat4 translation = interpolatePosition(animationTime);
        glm::mat4 rotation = interpolateRotation(animationTime);
        glm::mat4 scale = interpolateScaling(animationTime);
        localTransform = translation * rotation * scale;
    }

    // Animation implementation
    Bone* Animation::findBone(const std::string& name) {
        auto iter = std::find_if(bones.begin(), bones.end(),
            [&](const Bone& bone) { return bone.name == name; });
        return iter == bones.end() ? nullptr : &(*iter);
    }

    // AnimationComponent implementation
    AnimationComponent::AnimationComponent() {
        // Initialize bone matrices to identity
        m_finalBoneMatrices.reserve(MAX_BONES);
        for (int i = 0; i < MAX_BONES; i++) {
            m_finalBoneMatrices.push_back(glm::mat4(1.0f));
        }
    }

    void AnimationComponent::initialize() {
        Logger::getInstance().info("Animation Component initialized");
    }

    void AnimationComponent::cleanup() {
        stopAnimation();
        m_animations.clear();
        m_animationLayers.clear();
        Logger::getInstance().info("Animation Component cleaned up");
    }

    std::unique_ptr<Component> AnimationComponent::clone() const {
        auto clone = std::make_unique<AnimationComponent>();
        // Note: Deep copying animations would require more complex implementation
        // For now, just copy the basic state
        clone->m_currentAnimationName = m_currentAnimationName;
        clone->m_boneInfoMap = m_boneInfoMap;
        clone->m_globalInverseTransform = m_globalInverseTransform;
        clone->m_rootMotionEnabled = m_rootMotionEnabled;
        return clone;
    }

    void AnimationComponent::update(float deltaTime) {
        if (!m_currentState.playing && !m_transition.active) {
            return;
        }

        // Update transitions first
        if (m_transition.active) {
            updateTransition(deltaTime);
        } else {
            updateAnimation(deltaTime);
        }

        // Update animation layers
        for (auto& [name, layer] : m_animationLayers) {
            if (layer.playing) {
                updateAnimationLayer(layer, deltaTime);
            }
        }

        // Update root motion
        if (m_rootMotionEnabled) {
            updateRootMotion(deltaTime);
        }
    }

    void AnimationComponent::addAnimation(const std::string& name, std::unique_ptr<Animation> animation) {
        if (!animation) {
            Logger::getInstance().warning("Attempted to add null animation: " + name);
            return;
        }

        m_animations[name] = std::move(animation);
        Logger::getInstance().info("Added animation: " + name);
    }

    void AnimationComponent::playAnimation(const std::string& name, bool loop, float speed) {
        auto it = m_animations.find(name);
        if (it == m_animations.end()) {
            Logger::getInstance().warning("Animation not found: " + name);
            return;
        }

        // Stop current animation
        stopAnimation();

        // Set up new animation state
        m_currentAnimationName = name;
        m_currentState.animation = it->second.get();
        m_currentState.currentTime = 0.0f;
        m_currentState.weight = 1.0f;
        m_currentState.speed = speed;
        m_currentState.looping = loop;
        m_currentState.playing = true;
        m_currentState.finished = false;

        // Clear triggered events
        m_triggeredEvents.clear();

        // Call started callback
        if (m_onAnimationStarted) {
            m_onAnimationStarted(name);
        }

        Logger::getInstance().info("Playing animation: " + name);
    }

    void AnimationComponent::stopAnimation() {
        m_currentState.playing = false;
        m_currentState.currentTime = 0.0f;
        m_currentState.finished = false;
        m_transition.active = false;
        m_triggeredEvents.clear();
    }

    void AnimationComponent::pauseAnimation() {
        m_currentState.playing = false;
    }

    void AnimationComponent::resumeAnimation() {
        if (!m_currentState.finished && m_currentState.animation) {
            m_currentState.playing = true;
        }
    }

    void AnimationComponent::setAnimationSpeed(float speed) {
        m_currentState.speed = speed;
    }

    void AnimationComponent::setAnimationTime(float time) {
        if (m_currentState.animation) {
            m_currentState.currentTime = std::clamp(time, 0.0f, m_currentState.animation->duration);
        }
    }

    void AnimationComponent::blendToAnimation(const std::string& name, float transitionDuration) {
        auto it = m_animations.find(name);
        if (it == m_animations.end()) {
            Logger::getInstance().warning("Animation not found for blending: " + name);
            return;
        }

        if (m_currentState.animation && m_currentState.playing) {
            // Set up transition
            m_transition.fromAnimation = m_currentState.animation;
            m_transition.toAnimation = it->second.get();
            m_transition.duration = transitionDuration;
            m_transition.currentTime = 0.0f;
            m_transition.active = true;

            Logger::getInstance().info("Blending from " + m_currentAnimationName + " to " + name);
        } else {
            // No current animation, just play the new one
            playAnimation(name);
        }
    }

    void AnimationComponent::addAnimationLayer(const std::string& name, float weight) {
        auto it = m_animations.find(name);
        if (it == m_animations.end()) {
            Logger::getInstance().warning("Animation not found for layer: " + name);
            return;
        }

        AnimationState& layer = m_animationLayers[name];
        layer.animation = it->second.get();
        layer.weight = weight;
        layer.playing = true;
        layer.currentTime = 0.0f;
        layer.speed = 1.0f;
        layer.looping = true;
        layer.finished = false;
    }

    void AnimationComponent::setLayerWeight(const std::string& name, float weight) {
        auto it = m_animationLayers.find(name);
        if (it != m_animationLayers.end()) {
            it->second.weight = std::clamp(weight, 0.0f, 1.0f);
        }
    }

    void AnimationComponent::removeAnimationLayer(const std::string& name) {
        m_animationLayers.erase(name);
    }

    glm::mat4 AnimationComponent::getBoneTransform(const std::string& boneName) const {
        auto it = m_boneInfoMap.find(boneName);
        if (it != m_boneInfoMap.end() && it->second.id < m_finalBoneMatrices.size()) {
            return m_finalBoneMatrices[it->second.id];
        }
        return glm::mat4(1.0f);
    }

    glm::mat4 AnimationComponent::getBoneTransform(int boneIndex) const {
        if (boneIndex >= 0 && boneIndex < m_finalBoneMatrices.size()) {
            return m_finalBoneMatrices[boneIndex];
        }
        return glm::mat4(1.0f);
    }

    void AnimationComponent::setBoneTransform(const std::string& boneName, const glm::mat4& transform) {
        auto it = m_boneInfoMap.find(boneName);
        if (it != m_boneInfoMap.end() && it->second.id < m_finalBoneMatrices.size()) {
            m_finalBoneMatrices[it->second.id] = transform;
        }
    }

    void AnimationComponent::setBoneTransform(int boneIndex, const glm::mat4& transform) {
        if (boneIndex >= 0 && boneIndex < m_finalBoneMatrices.size()) {
            m_finalBoneMatrices[boneIndex] = transform;
        }
    }

    float AnimationComponent::getDuration() const {
        return m_currentState.animation ? m_currentState.animation->duration : 0.0f;
    }

    void AnimationComponent::setBoneInfoMap(const std::map<std::string, BoneInfo>& boneInfoMap) {
        m_boneInfoMap = boneInfoMap;
        
        // Resize bone matrices if needed
        int maxBoneId = 0;
        for (const auto& [name, info] : boneInfoMap) {
            maxBoneId = std::max(maxBoneId, info.id);
        }
        
        if (maxBoneId >= m_finalBoneMatrices.size()) {
            m_finalBoneMatrices.resize(maxBoneId + 1, glm::mat4(1.0f));
        }
    }

    void AnimationComponent::setAnimationFinishedCallback(AnimationEventCallback callback) {
        m_onAnimationFinished = callback;
    }

    void AnimationComponent::setAnimationStartedCallback(AnimationEventCallback callback) {
        m_onAnimationStarted = callback;
    }

    void AnimationComponent::addAnimationEvent(float time, const std::string& eventName) {
        m_animationEvents[time].push_back(eventName);
    }

    void AnimationComponent::updateAnimation(float deltaTime) {
        if (!m_currentState.animation || !m_currentState.playing) {
            return;
        }

        float previousTime = m_currentState.currentTime;
        m_currentState.currentTime += deltaTime * m_currentState.speed;

        // Handle animation end
        if (m_currentState.currentTime >= m_currentState.animation->duration) {
            if (m_currentState.looping) {
                m_currentState.currentTime = 0.0f;
                m_triggeredEvents.clear();
            } else {
                m_currentState.currentTime = m_currentState.animation->duration;
                m_currentState.playing = false;
                m_currentState.finished = true;
                
                if (m_onAnimationFinished) {
                    m_onAnimationFinished(m_currentAnimationName);
                }
            }
        }

        // Calculate bone transforms
        if (m_currentState.animation) {
            calculateBoneTransform(m_currentState.animation, m_currentState.currentTime, 
                                 nullptr, m_globalInverseTransform);
        }

        // Process animation events
        processAnimationEvents(previousTime, m_currentState.currentTime);
    }

    void AnimationComponent::updateTransition(float deltaTime) {
        m_transition.currentTime += deltaTime;
        float blendFactor = std::clamp(m_transition.currentTime / m_transition.duration, 0.0f, 1.0f);

        if (blendFactor >= 1.0f) {
            // Transition complete, switch to new animation
            m_currentState.animation = m_transition.toAnimation;
            m_currentState.currentTime = 0.0f;
            m_currentState.playing = true;
            m_currentState.finished = false;
            m_transition.active = false;
            
            // Update current animation name
            for (const auto& [name, anim] : m_animations) {
                if (anim.get() == m_transition.toAnimation) {
                    m_currentAnimationName = name;
                    break;
                }
            }
        } else {
            // Blend between animations
            std::vector<glm::mat4> fromTransforms(MAX_BONES, glm::mat4(1.0f));
            std::vector<glm::mat4> toTransforms(MAX_BONES, glm::mat4(1.0f));
            
            // Calculate transforms for both animations
            if (m_transition.fromAnimation) {
                calculateBoneTransform(m_transition.fromAnimation, m_currentState.currentTime, 
                                     nullptr, m_globalInverseTransform);
                fromTransforms = m_finalBoneMatrices;
            }
            
            if (m_transition.toAnimation) {
                calculateBoneTransform(m_transition.toAnimation, 0.0f, 
                                     nullptr, m_globalInverseTransform);
                toTransforms = m_finalBoneMatrices;
            }
            
            // Blend the transforms
            blendBoneTransforms(fromTransforms, toTransforms, blendFactor, m_finalBoneMatrices);
        }
    }

    void AnimationComponent::updateAnimationLayer(AnimationState& state, float deltaTime) {
        if (!state.animation) return;

        state.currentTime += deltaTime * state.speed;
        
        if (state.currentTime >= state.animation->duration) {
            if (state.looping) {
                state.currentTime = 0.0f;
            } else {
                state.currentTime = state.animation->duration;
                state.playing = false;
                state.finished = true;
            }
        }

        // Calculate and blend layer transforms
        // This is a simplified implementation - in practice, you'd want more sophisticated blending
        std::vector<glm::mat4> layerTransforms(MAX_BONES, glm::mat4(1.0f));
        calculateBoneTransform(state.animation, state.currentTime, nullptr, m_globalInverseTransform);
        
        // Blend with main animation based on weight
        if (state.weight > 0.0f) {
            blendBoneTransforms(m_finalBoneMatrices, layerTransforms, state.weight, m_finalBoneMatrices);
        }
    }

    void AnimationComponent::calculateBoneTransform(Animation* animation, float animationTime,
                                                   const aiNode* node, const glm::mat4& parentTransform) {
        // Simplified implementation - in a real system, you'd traverse the Assimp node hierarchy
        // For now, just update all bones in the animation
        
        if (!animation) return;

        for (auto& bone : animation->bones) {
            bone.update(animationTime);
            
            auto it = m_boneInfoMap.find(bone.name);
            if (it != m_boneInfoMap.end()) {
                int boneIndex = it->second.id;
                if (boneIndex < m_finalBoneMatrices.size()) {
                    m_finalBoneMatrices[boneIndex] = parentTransform * bone.localTransform * it->second.offset;
                }
            }
        }
    }

    void AnimationComponent::blendBoneTransforms(const std::vector<glm::mat4>& transforms1,
                                               const std::vector<glm::mat4>& transforms2,
                                               float weight, std::vector<glm::mat4>& result) {
        size_t size = std::min({transforms1.size(), transforms2.size(), result.size()});
        
        for (size_t i = 0; i < size; ++i) {
            // Decompose matrices for proper blending
            glm::vec3 scale1, translation1, skew1;
            glm::vec4 perspective1;
            glm::quat orientation1;
            glm::decompose(transforms1[i], scale1, orientation1, translation1, skew1, perspective1);
            
            glm::vec3 scale2, translation2, skew2;
            glm::vec4 perspective2;
            glm::quat orientation2;
            glm::decompose(transforms2[i], scale2, orientation2, translation2, skew2, perspective2);
            
            // Blend components
            glm::vec3 blendedTranslation = glm::mix(translation1, translation2, weight);
            glm::quat blendedRotation = glm::slerp(orientation1, orientation2, weight);
            glm::vec3 blendedScale = glm::mix(scale1, scale2, weight);
            
            // Reconstruct matrix
            result[i] = glm::translate(glm::mat4(1.0f), blendedTranslation) *
                       glm::mat4_cast(blendedRotation) *
                       glm::scale(glm::mat4(1.0f), blendedScale);
        }
    }

    void AnimationComponent::processAnimationEvents(float previousTime, float currentTime) {
        for (const auto& [eventTime, events] : m_animationEvents) {
            if (eventTime > previousTime && eventTime <= currentTime) {
                for (const auto& eventName : events) {
                    if (m_triggeredEvents.find(eventName) == m_triggeredEvents.end()) {
                        m_triggeredEvents.insert(eventName);
                        // In a real implementation, you'd trigger the event here
                        Logger::getInstance().info("Animation event triggered: " + eventName);
                    }
                }
            }
        }
    }

    void AnimationComponent::updateRootMotion(float deltaTime) {
        // Simplified root motion implementation
        // In practice, you'd extract root bone movement from the animation
        if (m_currentState.animation && m_currentState.playing) {
            // This is a placeholder - real implementation would extract root bone delta
            m_rootMotionDelta = glm::mat4(1.0f);
        }
    }

    // Utility conversion methods
    glm::mat4 AnimationComponent::assimpToGlmMatrix(const aiMatrix4x4& from) {
        glm::mat4 to;
        to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
        to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
        to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
        to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
        return to;
    }

    glm::vec3 AnimationComponent::assimpToGlmVec3(const aiVector3D& vec) {
        return glm::vec3(vec.x, vec.y, vec.z);
    }

    glm::quat AnimationComponent::assimpToGlmQuat(const aiQuaternion& quat) {
        return glm::quat(quat.w, quat.x, quat.y, quat.z);
    }

} // namespace IKore
