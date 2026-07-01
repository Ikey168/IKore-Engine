#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D hdrScene;
uniform float exposure;

// HDR resolve (issue #235): exposure -> Narkowicz ACES filmic -> gamma. Mirrors
// src/render/ToneMapping.h (acesFilmic / applyExposure / resolveChannel), which is
// unit-tested. Gamma uses pow(1/2.2) to match the engine's bloom_combine.frag.
vec3 aces(vec3 x) {
    x = max(x, vec3(0.0));
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(hdrScene, TexCoord).rgb;
    vec3 mapped = aces(hdr * exposure);
    mapped = pow(mapped, vec3(1.0 / 2.2));
    FragColor = vec4(mapped, 1.0);
}
