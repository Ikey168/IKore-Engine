#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 FragPosLightSpace;
out mat3 TBN;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
uniform mat4 lightSpaceMatrix;

const int MAX_BONES = 100;
uniform mat4 finalBonesMatrices[MAX_BONES];

// GPU linear-blend skinning (issue #260). Mirrors skinPosition/skinDirection in
// src/render/Skinning.h, which is unit-tested: each influencing bone transforms
// the vertex and the results blend by weight; out-of-range bone ids are skipped
// and a vertex with no applicable influence stays rigid. The varying interface
// matches phong_shadows.vert, so this pairs with phong_shadows.frag unchanged.
void main() {
    vec4 skinnedPos = vec4(0.0);
    vec3 skinnedNormal = vec3(0.0);
    vec3 skinnedTangent = vec3(0.0);
    float applied = 0.0;

    for (int i = 0; i < 4; ++i) {
        int id = aBoneIds[i];
        if (id < 0 || id >= MAX_BONES || aWeights[i] <= 0.0) continue;
        mat4 boneMatrix = finalBonesMatrices[id];
        skinnedPos += (boneMatrix * vec4(aPos, 1.0)) * aWeights[i];
        skinnedNormal += mat3(boneMatrix) * aNormal * aWeights[i];
        skinnedTangent += mat3(boneMatrix) * aTangent * aWeights[i];
        applied += aWeights[i];
    }
    if (applied <= 0.0) {
        // Rigid fallback: no influences bound (matches the core's behavior).
        skinnedPos = vec4(aPos, 1.0);
        skinnedNormal = aNormal;
        skinnedTangent = aTangent;
    }

    FragPos = vec3(model * skinnedPos);
    Normal = normalMatrix * skinnedNormal;
    TexCoord = aTexCoord;

    // TBN for normal mapping (issue #238), built from the skinned tangent.
    vec3 N = normalize(Normal);
    vec3 T = normalize(normalMatrix * skinnedTangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);

    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
