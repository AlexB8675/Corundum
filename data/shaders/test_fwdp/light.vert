#version 460
#extension GL_ARB_separate_shader_objects: enable

layout (location = 0) in vec3 i_vertex;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uvs;
layout (location = 3) in vec3 i_tangent;
layout (location = 4) in vec3 i_bitangent;

struct InstanceData {
    vec4 color;
    mat4 model;
};

struct CameraData {
    mat4 projection;
    mat4 view;
    vec3 position;
    float _u0;
};

layout (location = 0) out VertexData {
    vec3 color;
};

layout (set = 0, binding = 0) uniform Uniforms {
    CameraData camera;
};

layout (set = 0, binding = 1) readonly buffer Instances {
    InstanceData[] instances;
};

void main() {
    InstanceData instance = instances[gl_InstanceIndex];
    color = vec3(instance.color);
    gl_Position = camera.projection * camera.view * instance.model * vec4(i_vertex, 1.0);
}
