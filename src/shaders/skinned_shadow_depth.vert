#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

const int MAX_BONES = 100;
uniform mat4 finalBonesMatrices[MAX_BONES];

// Skinned directional shadow-depth pass (issue #266). Applies the same linear-blend
// skinning as skinned_phong.vert - which mirrors skinPosition in
// src/render/Skinning.h line for line, including the out-of-range bone guard and the
// no-influence identity fallback - so a skinned mesh's shadow matches its animated
// pose instead of casting its bind pose. Only the position is skinned (depth pass);
// it pairs with the existing shadow_depth.frag unchanged.
void main() {
    vec4 skinnedPos = vec4(0.0);
    float applied = 0.0;

    for (int i = 0; i < 4; ++i) {
        int id = aBoneIds[i];
        if (id < 0 || id >= MAX_BONES || aWeights[i] <= 0.0) continue;
        skinnedPos += (finalBonesMatrices[id] * vec4(aPos, 1.0)) * aWeights[i];
        applied += aWeights[i];
    }
    if (applied <= 0.0) {
        // Rigid fallback: no influences bound (matches the core's behavior).
        skinnedPos = vec4(aPos, 1.0);
    }

    gl_Position = lightSpaceMatrix * model * skinnedPos;
}
