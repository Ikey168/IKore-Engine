#version 330 core

in vec4 ParticleColor;
in float ParticleRotation;

uniform sampler2D particleTexture;

out vec4 FragColor;

void main() {
    // Get texture coordinates for the point
    vec2 texCoord = gl_PointCoord;
    
    // Apply rotation if needed
    if (ParticleRotation != 0.0) {
        // Convert to centered coordinates (-0.5 to 0.5)
        vec2 centeredCoord = texCoord - 0.5;
        
        // Apply rotation
        float cosR = cos(radians(ParticleRotation));
        float sinR = sin(radians(ParticleRotation));
        mat2 rotationMatrix = mat2(cosR, -sinR, sinR, cosR);
        centeredCoord = rotationMatrix * centeredCoord;
        
        // Convert back to texture coordinates (0 to 1)
        texCoord = centeredCoord + 0.5;
    }
    
    // Sample the particle texture
    vec4 texColor = texture(particleTexture, texCoord);
    
    // Combine texture color with particle color
    FragColor = texColor * ParticleColor;
    
    // Discard transparent pixels to improve performance
    if (FragColor.a < 0.01) {
        discard;
    }
}
