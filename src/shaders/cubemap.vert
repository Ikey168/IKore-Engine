#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 LocalPos;

uniform mat4 projection;
uniform mat4 view;

// Renders a unit cube once per face for the IBL convolution passes (issue #270). The
// local position is the sampling direction the fragment shader convolves around.
void main() {
    LocalPos = aPos;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
