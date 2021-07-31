#version 460
#extension GL_EXT_nonuniform_qualifier: enable

layout (location = 0) out vec4 pixel;

layout (location = 0) in VertexData {
    vec4 shadow_frag_pos;
    vec3 frag_pos;
    vec2 uvs;
};

layout (set = 0, binding = 2) uniform sampler2D   shadow_map;
layout (set = 0, binding = 3) uniform sampler2D[] textures;

layout (push_constant) uniform Constants {
    uint model_index;
    uint diffuse_index;
    uint normal_index;
    uint specular_index;
};

float calculate_shadow(vec3 shadow_proj) {
    shadow_proj = shadow_proj * 0.5 + 0.5;
    if (shadow_proj.z > 1.0) {
        return 0.0;
    }
    float closest = texture(shadow_map, shadow_proj.xy).r;
    float current = shadow_proj.z;
    return current > closest ? 1.0 : 0.0;
}

void main() {
    vec4 color = texture(textures[diffuse_index], uvs);
    vec3 ambient = 0.15 * vec3(color);
    float shadow = calculate_shadow(shadow_frag_pos.xyz / shadow_frag_pos.w);
    pixel = vec4((ambient + (1.0 - shadow)) * ambient, color.a);
}
