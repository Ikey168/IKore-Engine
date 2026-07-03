#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

// Fullscreen quad for the split-sum BRDF integration LUT (issue #270). Same quad
// layout the post-processing effects use (vec2 position, vec2 uv).
void main() {
    TexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
