#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D sceneColor;
uniform sampler2D ssao;
uniform float intensity;

// Apply the (blurred) ambient-occlusion term to the lit scene (issue #259).
// Mirrors ssaoCombineWeight in src/render/SsaoKernel.h: mix(1, ao, intensity)
// with intensity clamped, so 0 is a no-op and 1 applies the full AO term.
void main() {
    vec4 scene = texture(sceneColor, TexCoord);
    float ao = texture(ssao, TexCoord).r;
    float weight = mix(1.0, ao, clamp(intensity, 0.0, 1.0));
    FragColor = vec4(scene.rgb * weight, scene.a);
}
