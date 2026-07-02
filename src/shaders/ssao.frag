#version 330 core
out float FragColor;

in vec2 TexCoord;

uniform sampler2D gDepth;
uniform sampler2D texNoise;

uniform vec3 samples[64];
uniform mat4 projection;
uniform mat4 invProjection;
uniform vec2 noiseScale; // screen / 4 (mirrors ssaoNoiseScale in SsaoKernel.h)

uniform float radius;
uniform float bias;
uniform int sampleCount;

// SSAO from the depth buffer (issue #259). Mirrors src/render/SsaoKernel.h, which
// is unit-tested: the hemisphere kernel comes from generateSsaoKernel, the range
// falloff and per-sample test are ssaoRangeCheck / ssaoSampleOcclusion, and the
// final term is ssaoResolve. View-space normals are reconstructed from depth
// derivatives, so the depth half of a G-buffer is all this pass needs (no separate
// normals prepass, no second scene render).

vec3 viewPosAt(vec2 uv) {
    float depth = texture(gDepth, uv).x;
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = invProjection * clip;
    return view.xyz / view.w;
}

void main() {
    vec3 fragPos = viewPosAt(TexCoord);

    // View-space normal from screen-space derivatives of the reconstructed
    // position (faceted per pixel-quad, which AO tolerates well).
    vec3 normal = normalize(cross(dFdx(fragPos), dFdy(fragPos)));

    vec3 randomVec = normalize(texture(texNoise, TexCoord * noiseScale).xyz);

    // Gram-Schmidt TBN around the reconstructed normal.
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        // Kernel sample around the fragment, in view space.
        vec3 samplePos = fragPos + (TBN * samples[i]) * radius;

        // Project it to screen space to find what the depth buffer has there.
        vec4 offset = projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        float sampleViewZ = viewPosAt(offset.xy).z;

        // ssaoSampleOcclusion: occluded when the surface is in front of the sample
        // point (bias fights acne), faded by the ssaoRangeCheck falloff.
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleViewZ));
        occlusion += (sampleViewZ >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    // ssaoResolve: 1 fully lit, 0 fully occluded.
    FragColor = 1.0 - (occlusion / float(sampleCount));
}
