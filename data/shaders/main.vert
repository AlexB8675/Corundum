#version 460

layout (location = 0) in vec3 iposition;
layout (location = 1) in vec3 icolor;

layout (location = 0) out VertexData {
    vec3 color;
};

layout (set = 0, binding = 0) uniform Camera {
    mat4 proj_view;
};

void main() {
    gl_Position = proj_view * vec4(iposition, 1.0);
    color = icolor;
}