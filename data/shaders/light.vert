#version 460

#extension GL_ARB_separate_shader_objects: enable

layout (location = 0) in vec3 i_vertex;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uvs;
layout (location = 3) in vec3 i_tangent;
layout (location = 4) in vec3 i_bitangent;

layout (location = 0) out flat uint instance;

layout (set = 0, binding = 0) uniform Uniforms {
    mat4 projection;
    mat4 view;
    mat4 _u0;
};

layout (set = 0, binding = 1) buffer readonly Models {
    mat4[] models;
};

void main() {
    gl_Position = projection * view * models[gl_InstanceIndex] * vec4(i_vertex, 1.0);
    instance = gl_InstanceIndex;
}
