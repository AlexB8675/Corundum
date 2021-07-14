#version 460

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

layout (location = 0) out VertexData {
    vec3 color;
} vout;

void main() {
    gl_Position = vec4(position, 1.0);
    vout.color = color;
}