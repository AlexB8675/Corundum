#version 460
#extension GL_ARB_separate_shader_objects: enable

layout (location = 0) in vec3 i_vertex;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uvs;
layout (location = 3) in vec3 i_tangent;
layout (location = 4) in vec3 i_bitangent;

struct CameraData {
    mat4 projection;
    mat4 view;
    vec3 position;
};

layout (set = 0, binding = 0) uniform Uniforms {
    CameraData camera;
};

layout (set = 0, binding = 1) readonly buffer Models {
    mat4[] models;
};

layout (push_constant) uniform Constants {
    uint model_index;
};

void main() {
    const mat4 model = models[model_index + gl_InstanceIndex];
    const vec3 frag_pos = vec3(model * vec4(i_vertex, 1.0));
    gl_Position = camera.projection * camera.view * vec4(frag_pos, 1.0);
}
