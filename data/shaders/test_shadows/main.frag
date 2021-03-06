#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#define ambient_factor 0.01

const mat4 shadow_bias = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0);

struct DirectionalLight {
    vec3 direction;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;
    vec3 falloff;
    vec3 diffuse;
    vec3 specular;
};

layout (location = 0) out vec4 pixel;

layout (location = 0) in VertexData {
    mat3 TBN;
    vec4 light_frag_pos;
    vec3 frag_pos;
    vec3 i_normal;
    vec2 uvs;
};

layout (set = 0, binding = 2) uniform sampler2D[] textures;

layout (set = 1, binding = 0) uniform ViewPos {
    vec3 view_pos;
};

layout (std430, set = 1, binding = 1) buffer readonly DirectionalLights {
    DirectionalLight[] directional_lights;
};

layout (std430, set = 1, binding = 2) buffer readonly PointLights {
    PointLight[] point_lights;
};

layout (set = 1, binding = 3) uniform sampler2D shadow;

layout (push_constant) uniform Constants {
    uint model_index;
    uint diffuse_index;
    uint normal_index;
    uint specular_index;
};

vec3 calculate_point_light(PointLight, vec3, vec3, vec3, vec3);
vec3 calculate_shadow(vec3, vec3, vec3);
vec3 filter_pcf(vec3, vec3);

void main() {
    const vec3 normal = normal_index != 0 ? normalize(TBN * (2.0 * texture(textures[normal_index], uvs).rgb - 1.0)) : i_normal;
    const vec3 albedo = texture(textures[diffuse_index], uvs).rgb;
    const vec3 view_dir = normalize(view_pos - frag_pos);

    vec3 color = albedo * ambient_factor;
    for (uint i = 0; i < point_lights.length(); ++i) {
        color += calculate_point_light(point_lights[i], albedo, normal, frag_pos, view_dir);
    }
    color = filter_pcf(color, albedo);
    pixel = vec4(color, 1.0);
}

vec3 calculate_point_light(PointLight light, vec3 color, vec3 normal, vec3 frag_pos, vec3 view_dir) {
    const vec3 light_dir = normalize(vec3(light.position) - frag_pos);
    // Diffuse.
    const float diffuse_comp = max(dot(light_dir, normal), 0.0);
    // Specular.
    const vec3 halfway_dir = normalize(light_dir + view_dir);
    const float specular_comp = pow(max(dot(halfway_dir, normal), 0.0), 32);

    // Attenuation.
    const float distance = length(vec3(light.position) - frag_pos);
    const float attenuation = 1.0 / (light.falloff.x + (light.falloff.y * distance) + (light.falloff.z * (distance * distance)));

    // Combine.
    const vec3 result_diffuse = vec3(light.diffuse) * diffuse_comp * color;
    const vec3 result_specular = vec3(light.specular) * specular_comp * texture(textures[specular_index], uvs).rgb;

    return (result_diffuse + result_specular) * attenuation;
}

vec3 calculate_shadow(vec3 color, vec3 albedo, vec2 offset) {
    const vec4 shadow_coords = shadow_bias * (light_frag_pos / light_frag_pos.w);
    const vec2 texel = vec2(shadow_coords.x, 1.0 - shadow_coords.y);
    const float bias = 0.0025;
    const float current = shadow_coords.z + bias;
    if (current > -1.0 && current < 1.0) {
        const float closest = texture(shadow, texel + offset).r;
        if (shadow_coords.w > 0.0 && current > closest) {
            return ambient_factor * albedo;
        }
    }
    return color;
}

vec3 filter_pcf(vec3 color, vec3 albedo) {
    ivec2 shadow_size = textureSize(shadow, 0).xy;
    float scale = 1.5;
    float dx = scale * 1.0 / float(shadow_size.x);
    float dy = scale * 1.0 / float(shadow_size.y);

    vec3 shadow_factor = vec3(0.0);
    int count = 0;
    int range = 2;

    for (int x = -range; x <= range; x++) {
        for (int y = -range; y <= range; y++) {
            shadow_factor += calculate_shadow(color, albedo, vec2(dx * x, dy * y));
            count++;
        }

    }
    return shadow_factor / count;
}
