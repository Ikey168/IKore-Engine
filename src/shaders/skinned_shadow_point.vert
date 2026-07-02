#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

uniform mat4 model;

const int MAX_BONES = 100;
uniform mat4 finalBonesMatrices[MAX_BONES];

// Skinned point-light shadow pass (issue #266). Same linear-blend skinning as
// skinned_shadow_depth.vert / skinned_phong.vert (mirroring skinPosition in
// src/render/Skinning.h, with the same out-of-range guard and rigid fallback), but
// emitting the world-space skinned position; the point-shadow geometry stage
// projects it to each cubemap face (as shadow_point.vert does for static meshes).
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
        skinnedPos = vec4(aPos, 1.0);
    }

    gl_Position = model * skinnedPos;
}
