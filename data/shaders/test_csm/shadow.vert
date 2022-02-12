#version 460

layout (location = 0) in vec3 i_vertex;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uvs;
layout (location = 3) in vec3 i_tangent;
layout (location = 4) in vec3 i_bitangent;

layout (location = 0) out vec2 uvs;

layout (std430, set = 0, binding = 0) buffer readonly Models {
    mat4[] model;
};

layout (push_constant) uniform Constants {
    uint model_index;
    uint diffuse_index;
};

void main() {
    uvs = i_uvs;
    gl_Position = model[model_index + gl_InstanceIndex] * vec4(i_vertex, 1.0);
}
