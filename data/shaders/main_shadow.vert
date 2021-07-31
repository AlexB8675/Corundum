#version 460

layout (location = 0) in vec3 ivertex;
layout (location = 1) in vec3 inormal;
layout (location = 2) in vec2 iuvs;
layout (location = 3) in vec3 itangents;
layout (location = 4) in vec3 ibitangents;

layout (location = 0) out VertexData {
    vec4 shadow_frag_pos;
    vec3 light_pos;
    vec3 normal;
    vec3 frag_pos;
    vec2 uvs;
} vout;

layout (set = 0, binding = 0) uniform Camera {
    mat4 shadow_proj_view;
    mat4 proj_view;
    vec3 light_pos;
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
    vout.frag_pos = vec3(model[model_index] * vec4(ivertex, 1.0));
    vout.uvs = iuvs;
    vout.normal = transpose(inverse(mat3(model[model_index]))) * inormal;
    vout.light_pos = light_pos;
    vout.shadow_frag_pos = shadow_proj_view * vec4(vout.frag_pos, 1.0);
    gl_Position = proj_view * vec4(vout.frag_pos, 1.0);
}
