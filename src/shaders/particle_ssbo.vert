#version 430

// SSBO-pull particle vertex shader (issue #267). Renders particles straight from
// the compute buffer with no per-frame readback: the draw is glDrawArrays(GL_POINTS,
// 0, maxParticles) with no vertex attributes, and each invocation pulls its particle
// by gl_VertexID from the same std430 ParticleBuffer the update/emit compute shaders
// write. Dead particles are culled by clipping them out with a zero point size.
//
// Color/size come from the emitter config faded by life, matching the CPU render
// path (mix(endColor, startColor, life) / mix(endSize, startSize, life)) so the two
// paths look identical. Pairs with particle.frag unchanged.

struct Particle {
    vec3 position;
    float life;
    vec3 velocity;
    float size;
    vec4 color;
    vec3 acceleration;
    float startLife;
    vec2 rotation;
    float mass;
    float padding;
};

struct EmitterConfig {
    vec3 position;            float padding1;
    vec3 positionVariance;   float padding2;
    vec3 velocity;           float padding3;
    vec3 velocityVariance;   float padding4;
    vec3 acceleration;       float padding5;
    vec3 accelerationVariance; float padding6;
    vec4 startColor;
    vec4 endColor;
    vec4 colorVariance;
    float startSize;  float endSize;  float sizeVariance;  float padding7;
    float startLife;  float lifeVariance;  float emissionRate;  float maxParticles;
    float mass;  float massVariance;  float padding8;  float padding9;
    bool looping;  bool worldSpace;  float padding10;  float padding11;
};

layout(std430, binding = 0) restrict readonly buffer ParticleBuffer {
    Particle particles[];
};

layout(std140, binding = 0) uniform EmitterBuffer {
    EmitterConfig emitter;
};

uniform mat4 view;
uniform mat4 projection;

out vec4 ParticleColor;
out float ParticleRotation;

void main() {
    Particle p = particles[gl_VertexID];

    // Cull dead particles: clip them out and give them no footprint.
    if (p.life <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // outside the clip volume
        gl_PointSize = 0.0;
        ParticleColor = vec4(0.0);
        ParticleRotation = 0.0;
        return;
    }

    float lifeRatio = p.life;
    ParticleColor = mix(emitter.endColor, emitter.startColor, lifeRatio);
    ParticleRotation = p.rotation.x;
    float currentSize = mix(emitter.endSize, emitter.startSize, lifeRatio);

    vec4 viewPos = view * vec4(p.position, 1.0);
    gl_Position = projection * viewPos;

    float dist = length(viewPos.xyz);
    gl_PointSize = max(1.0, (currentSize * 100.0) / dist);
}
