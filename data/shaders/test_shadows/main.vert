#version 460

layout (location = 0) in vec3 i_vertex;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uvs;
layout (location = 3) in vec3 i_tangent;
layout (location = 4) in vec3 i_bitangent;

layout (location = 0) out VertexData {
    mat3 TBN;
    vec4 light_frag_pos;
    vec3 frag_pos;
    vec3 normal;
    vec2 uvs;
};

layout (set = 0, binding = 0) uniform Uniforms {
    mat4 proj_view;
    mat4 light_proj_view;
};

layout (set = 0, binding = 1) buffer readonly Models {
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
    light_frag_pos = light_proj_view * vec4(frag_pos, 1.0);
    normal = i_normal;
    uvs = i_uvs;
    gl_Position = proj_view * vec4(frag_pos, 1.0);
}
