#version 460

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
};

layout (set = 0, binding = 0) uniform Camera {
    mat4 proj_view;
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
    TBN = mat3(normalize(vec3(model[model_index] * vec4(i_tangent, 1.0))),
               normalize(vec3(model[model_index] * vec4(i_bitangent, 1.0))),
               normalize(vec3(model[model_index] * vec4(i_normal, 1.0))));
    frag_pos = vec3(model[model_index] * vec4(i_vertex, 1.0));
    normal = i_normal;
    uvs = i_uvs;
    gl_Position = proj_view * vec4(frag_pos, 1.0);
}
