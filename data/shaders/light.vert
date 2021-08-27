#version 460

layout (location = 0) in vec3 i_vertex;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uvs;
layout (location = 3) in vec3 i_tangent;
layout (location = 4) in vec3 i_bitangent;

layout (location = 0) out flat uint instance;

layout (set = 0, binding = 0) uniform Uniforms {
    mat4 proj_view;
};

layout (set = 0, binding = 1) buffer readonly Models {
    mat4[] model;
};

void main() {
    instance = gl_InstanceIndex;
    gl_Position = proj_view * model[instance] * vec4(i_vertex, 1.0);
}
