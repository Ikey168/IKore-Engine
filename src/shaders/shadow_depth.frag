#version 330 core

// Fragment shader for depth-only rendering (shadow mapping)
// This shader doesn't output any color - depth is written automatically

void main() {
    // We don't need to write anything here
    // The depth buffer is automatically written by OpenGL
    // This shader exists mainly for compatibility and potential future use
}
