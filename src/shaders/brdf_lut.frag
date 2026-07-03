#version 330 core
out vec2 FragColor; // (scale, bias) written into an RG LUT

in vec2 TexCoord;

const float PI = 3.14159265359;

// Split-sum environment BRDF integration (issue #270). Mirrors
// render::integrateBrdfLut / hammersley / importanceSampleGGX / geometrySmithIBL in
// src/render/IblBrdf.h, which is unit-tested; GLSL is not compiled in CI, so keeping
// the math identical to the tested C++ is the safeguard against drift.
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// Tangent-space (N = +Z) GGX-importance-sampled half-vector.
vec3 ImportanceSampleGGX(vec2 Xi, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float GeometrySchlickGGX(float NdotX, float k) {
    return NdotX / (NdotX * (1.0 - k) + k);
}

float GeometrySmithIBL(float NdotV, float NdotL, float roughness) {
    float k = (roughness * roughness) / 2.0; // IBL remap (PbrBrdf.h roughnessToKIBL)
    return GeometrySchlickGGX(NdotV, k) * GeometrySchlickGGX(NdotL, k);
}

vec2 IntegrateBRDF(float NdotV, float roughness) {
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    float A = 0.0;
    float B = 0.0;
    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, roughness);
        vec3 L = 2.0 * dot(V, H) * H - V;

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0) {
            float G = GeometrySmithIBL(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV + 1e-8);
            float Fc = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    return vec2(A, B) / float(SAMPLE_COUNT);
}

void main() {
    FragColor = IntegrateBRDF(TexCoord.x, TexCoord.y);
}
