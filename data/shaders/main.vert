#version 460

layout (location = 0) in vec3 iposition;
layout (location = 1) in vec3 icolor;

layout (location = 0) out VertexData {
    vec3 color;
};

void main() {
    gl_Position = vec4(iposition, 1.0);
    color = icolor;
}