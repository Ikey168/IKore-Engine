#version 330 core

out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;
uniform float intensity;

void main()
{    
    vec3 color = texture(skybox, TexCoords).rgb;
    
    // Apply intensity/brightness adjustment
    color *= intensity;
    
    // Optional: Simple tone mapping for HDR skyboxes
    // color = color / (color + vec3(1.0));
    
    // Optional: Gamma correction
    // color = pow(color, vec3(1.0/2.2));
    
    FragColor = vec4(color, 1.0);
}
