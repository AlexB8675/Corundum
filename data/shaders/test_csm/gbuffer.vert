#version 460
#extension GL_ARB_separate_shader_objects: enable

layout (location = 0) in vec3 i_vertex;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uvs;
layout (location = 3) in vec3 i_tangent;
layout (location = 4) in vec3 i_bitangent;

layout (location = 0) out VertexData {
    mat3 TBN;
    vec3 frag_pos;
    vec3 normal;
    vec2 uvs;
    float view_depth;
};

layout (set = 0, binding = 0) uniform Uniforms {
    mat4 projection;
    mat4 view;
    vec4 position;
};

layout (std430, set = 0, binding = 1) buffer readonly Models {
    mat4[] model;
};

layout (push_constant) uniform Constants {
    uint model_index;
    uint diffuse_index;
    uint normal_index;
    uint specular_index;
};

void main() {
    const mat4 t_model = model[model_index + gl_InstanceIndex];
    vec3 T = normalize(vec3(t_model * vec4(i_tangent, 0.0)));
    vec3 N = normalize(vec3(t_model * vec4(i_normal, 0.0)));
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);
    frag_pos = vec3(t_model * vec4(i_vertex, 1.0));
    normal = mat3(transpose(inverse(t_model))) * i_normal;
    uvs = i_uvs;
    view_depth = (view * vec4(frag_pos, 1.0)).z;
    gl_Position = projection * view * vec4(frag_pos, 1.0);
}
