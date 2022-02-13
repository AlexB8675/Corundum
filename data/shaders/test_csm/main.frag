#version 460
#extension GL_EXT_nonuniform_qualifier : enable

#define ambient_factor 0.02
#define max_shadow_cascades 16
#define shadow_cascades 4

const mat4 shadow_bias = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0);

struct Cascade {
    mat4 pv;
    float split;
};

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
    vec3 frag_pos;
    vec3 i_normal;
    vec2 uvs;
    float view_depth;
};

layout (set = 0, binding = 2) uniform sampler2D[] textures;

layout (set = 1, binding = 0) uniform ViewPos {
    vec3 view_pos;
};

layout (set = 1, binding = 1) uniform DirectionalLights {
    DirectionalLight[4] directional_lights;
};

layout (std430, set = 1, binding = 2) buffer readonly PointLights {
    PointLight[] point_lights;
};

layout (set = 1, binding = 3) uniform sampler2DArray shadow;

layout (set = 1, binding = 4) uniform Cascades {
    Cascade cascades[max_shadow_cascades];
};

layout (push_constant) uniform Constants {
    uint model_index;
    uint diffuse_index;
    uint normal_index;
    uint specular_index;
};

vec3 calculate_point_light(PointLight, vec3, vec3, vec3);
vec3 calculate_shadow(vec3, vec3, vec3, vec2, uint);
vec3 filter_pcf(vec3, vec3, vec3, uint);
uint calculate_layer();

void main() {
    const vec3 normal = normal_index != 0 ? normalize(TBN * (2.0 * texture(textures[normal_index], uvs).rgb - 1.0)) : i_normal;
    const vec3 view_dir = normalize(view_pos - frag_pos);
    const vec4 albedo = texture(textures[diffuse_index], uvs);

    vec3 color = vec3(albedo) * ambient_factor;
    for (uint i = 0; i < point_lights.length(); ++i) {
        color += calculate_point_light(point_lights[i], vec3(albedo), normal, view_dir);
    }
    const uint layer = calculate_layer();
    color = filter_pcf(color, normal, vec3(albedo), layer);
    pixel = vec4(color, albedo.a);
}

vec3 calculate_point_light(PointLight light, vec3 color, vec3 normal, vec3 view_dir) {
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
    const vec3 result_specular = vec3(light.specular) * specular_comp * (texture(textures[specular_index], uvs).rgb * color);

    return (result_diffuse + result_specular) * attenuation;
}

uint calculate_layer() {
    uint layer = shadow_cascades - 1;
    for (uint i = 0; i < shadow_cascades; ++i) {
        if (abs(view_depth) < cascades[i].split) {
            layer = i;
            break;
        }
    }
    return layer;
}

vec3 calculate_shadow(vec3 color, vec3 normal, vec3 albedo, vec2 offset, uint layer) {
    const vec4 light_frag_pos = (shadow_bias * cascades[layer].pv) * vec4(frag_pos, 1.0);
    const vec4 shadow_coords = light_frag_pos / light_frag_pos.w;
    const vec2 texel = vec2(shadow_coords.x, 1.0 - shadow_coords.y);
    const float bias = 0.00025 * (1 / (cascades[layer].split * 0.5));
    const float current = shadow_coords.z + bias;
    if (shadow_coords.z > -1.0 && shadow_coords.z < 1.0) {
        const float closest = texture(shadow, vec3(texel + offset, layer)).r;
        if (shadow_coords.w > 0 && current > closest) {
            return ambient_factor * albedo;
        }
    }
    return color;
}

vec3 filter_pcf(vec3 color, vec3 normal, vec3 albedo, uint layer) {
    const ivec2 shadow_size = textureSize(shadow, 0).xy;
    const float dx = 0.75 / float(shadow_size.x);
    const float dy = 0.75 / float(shadow_size.y);

    vec3 shadow_factor = vec3(0.0);
    int count = 0;
    int range = 1;
    for (int x = -range; x <= range; x++) {
        for (int y = -range; y <= range; y++) {
            shadow_factor += calculate_shadow(color, normal, albedo, vec2(dx * x, dy * y), layer);
            count++;
        }

    }
    return shadow_factor / count;
}
