#version 330 core
in vec4 FragPos;

uniform vec3 lightPos;
uniform float farPlane;

void main() {
    // Get distance between fragment and light source
    float lightDistance = length(FragPos.xyz - lightPos);
    
    // Map to [0;1] range by dividing by farPlane
    lightDistance = lightDistance / farPlane;
    
    // Write this as modified depth
    gl_FragDepth = lightDistance;
}
