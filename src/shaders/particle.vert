#version 330 core

layout (location = 0) in vec3 aPosition;   // Particle position
layout (location = 1) in float aSize;      // Particle size
layout (location = 2) in vec4 aColor;      // Particle color
layout (location = 3) in float aRotation;  // Particle rotation

uniform mat4 view;
uniform mat4 projection;

out vec4 ParticleColor;
out float ParticleRotation;

void main() {
    // Pass color and rotation to fragment shader
    ParticleColor = aColor;
    ParticleRotation = aRotation;
    
    // Transform particle position to screen space
    vec4 worldPos = vec4(aPosition, 1.0);
    vec4 viewPos = view * worldPos;
    
    gl_Position = projection * viewPos;
    
    // Set point size based on distance and particle size
    float distance = length(viewPos.xyz);
    gl_PointSize = max(1.0, (aSize * 100.0) / distance);
}
