#version 460
#extension GL_EXT_nonuniform_qualifier: enable

layout (location = 0) out vec4 o_position;
layout (location = 1) out vec4 o_normals;
layout (location = 2) out vec4 o_specular;
layout (location = 3) out vec4 o_albedo;

layout (location = 0) in VertexData {
    mat3 TBN;
    vec3 frag_pos;
    vec3 normal;
    vec2 uvs;
};

layout (set = 0, binding = 2) uniform sampler2D[] textures;

layout (push_constant) uniform Constants {
    uint model_index;
    uint diffuse_index;
    uint normal_index;
    uint specular_index;
};

void main() {
    o_position = vec4(frag_pos, 1.0);
    o_normals = vec4(normalize(TBN * (2.0 * texture(textures[normal_index], uvs).rgb - 1.0)), 1.0);
    o_specular = texture(textures[specular_index], uvs);
    o_albedo = texture(textures[diffuse_index], uvs);
}
