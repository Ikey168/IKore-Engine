#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform float bloomIntensity;

// When the ACES resolve runs at the end of the chain (issue #268), bloom must leave
// the image in linear HDR so the exposure + ACES + gamma resolve applies exactly
// once. In that mode this shader only adds the bloom and emits linear HDR - no
// Reinhard, no gamma. When off (the default), it keeps the original Reinhard + gamma
// so the LDR chain is byte-identical.
uniform bool toneMapResolve;

void main() {
    vec3 hdrColor = texture(scene, TexCoord).rgb;
    vec3 bloomColor = texture(bloomBlur, TexCoord).rgb;

    // Additive blending
    hdrColor += bloomColor * bloomIntensity;

    if (toneMapResolve) {
        // Leave linear HDR for the single end-of-chain ACES + gamma resolve.
        FragColor = vec4(hdrColor, 1.0);
        return;
    }

    // Tone mapping (Reinhard)
    vec3 result = hdrColor / (hdrColor + vec3(1.0));

    // Gamma correction
    result = pow(result, vec3(1.0/2.2));

    FragColor = vec4(result, 1.0);
}
