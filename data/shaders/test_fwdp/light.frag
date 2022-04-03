#version 460
#extension GL_ARB_separate_shader_objects: enable

layout (location = 0) out vec4 pixel;

layout (location = 0) in VertexData {
    vec3 color;
};

void main() {
    pixel = vec4(color, 1.0);
}