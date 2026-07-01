#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent; // supplied by the mesh VAO (issue #238)

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 FragPosLightSpace;
out mat3 TBN; // tangent-space -> world basis for normal mapping (issue #238)

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
uniform mat4 lightSpaceMatrix;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalMatrix * aNormal;
    TexCoord = aTexCoord;

    // Build the TBN basis. Mirrors orthonormalizeTangent / computeBitangent in
    // src/render/NormalMapping.h: Gram-Schmidt the tangent against the normal, then
    // take the right-handed bitangent. Only used by the fragment shader when a
    // normal map is bound, so meshes without tangents are unaffected.
    vec3 N = normalize(Normal);
    vec3 T = normalize(normalMatrix * aTangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);

    // Calculate position in light space for shadow mapping
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);

    gl_Position = projection * view * vec4(FragPos, 1.0);
}
