#version 460
#extension GL_ARB_separate_shader_objects: enable
#extension GL_EXT_nonuniform_qualifier: enable

#define tile_size 16
#define max_lights_per_tile 512
#define max_shadow_cascades 16
#define max_directional_lights 4
#define shadow_cascades 4
#define pcf_range 4
#define ambient_factor 0.025

const mat4 shadow_bias = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0);

struct DirectionalLight {
    vec3 direction;
    float _u0;
    vec3 diffuse;
    float _u1;
    vec3 specular;
    float _u2;
};

struct PointLight {
    vec3 position;
    float _u0;
    vec3 diffuse;
    float _u1;
    vec3 specular;
    float _u2;
    float radius;
    float[3] _u3;
};

struct Cascade {
    mat4 pv;
    float split;
};

struct LightVisibility {
    uint count;
    uint[max_lights_per_tile] indices;
};

layout (early_fragment_tests) in;

layout (location = 0) out vec4 pixel;

layout (location = 0) in VertexData {
    mat3 TBN;
    vec3 frag_pos;
    vec3 view_pos;
    vec3 i_normal;
    vec2 uvs;
    float view_depth;
};

layout (set = 0, binding = 2) uniform sampler2D[] textures;

layout (set = 1, binding = 0) buffer readonly PointLights {
    PointLight[] point_lights;
};

layout (set = 1, binding = 1) uniform DirectionalLights {
    DirectionalLight[max_directional_lights] directional_lights;
};

layout (set = 1, binding = 2) buffer readonly LightVisibilities {
    LightVisibility[] light_visibilities;
};

layout (set = 1, binding = 4) uniform Cascades {
    Cascade[max_shadow_cascades] cascades;
};

layout (set = 1, binding = 5) uniform sampler2DArray shadow;

layout (push_constant) uniform Indices {
    uint model_index;
    uint diffuse_index;
    uint normal_index;
    uint specular_index;
    uint directional_lights_count;
    ivec2 tiles;
};

vec3 fetch_normal_map();
vec3 calculate_directional_light(DirectionalLight light, vec3 albedo, vec3 normal, vec3 view_dir);
vec3 calculate_point_light(PointLight light, vec3 albedo, vec3 normal, vec3 view_dir);
uint calculate_layer();
vec3 sample_shadow(vec3 color, vec3 normal, vec3 light_dir, vec3 albedo, vec2 offset, uint layer);
vec3 compute_pcf(DirectionalLight light, vec3 color, vec3 normal, vec3 albedo, uint layer);

void main() {
    const vec3 normal = normalize(normal_index != 0 ? fetch_normal_map() : i_normal);
    const vec3 view_dir = normalize(view_pos - frag_pos);
    const vec3 albedo = vec3(texture(textures[diffuse_index], uvs));
    const ivec2 tile_id = ivec2(gl_FragCoord.xy / tile_size);
    const uint tile_index = tile_id.y * tiles.x + tile_id.x;
    const uint layer = calculate_layer();
    vec3 color = albedo * ambient_factor;
    for (uint i = 0; i < directional_lights_count; ++i) {
        color += calculate_directional_light(directional_lights[i], albedo, normal, view_dir);
        color = compute_pcf(directional_lights[i], color, normal, albedo, layer);
    }
    for (uint i = 0; i < light_visibilities[tile_index].count; ++i) {
        color += calculate_point_light(point_lights[light_visibilities[tile_index].indices[i]], albedo, normal, view_dir);
    }
    const float intensity = float(light_visibilities[tile_index].count) / max_lights_per_tile;
    pixel = vec4(vec3(intensity), 1.0);
    pixel = vec4(color, 1.0);
}

vec3 fetch_normal_map() {
    return TBN * (2.0 * texture(textures[normal_index], uvs).rgb - 1.0);
}

vec3 calculate_directional_light(DirectionalLight light, vec3 albedo, vec3 normal, vec3 view_dir) {
    const vec3 light_dir = normalize(light.direction);
    // Diffuse.
    const float diffuse_factor = max(dot(light_dir, normal), 0.0);
    const vec3 diffuse_color = light.diffuse * diffuse_factor * albedo;
    // Specular.
    const vec3 halfway_dir = normalize(light_dir + view_dir);
    const float specular_factor = pow(max(dot(halfway_dir, normal), 0.0), 32);
    const vec3 specular_color = light.specular * specular_factor * vec3(texture(textures[specular_index], uvs));

    return diffuse_color + specular_color;
}

vec3 calculate_point_light(PointLight light, vec3 albedo, vec3 normal, vec3 view_dir) {
    const vec3 light_dir = normalize(light.position - frag_pos);
    // Diffuse.
    const float diffuse_factor = max(dot(light_dir, normal), 0.0);
    const vec3 diffuse_color = light.diffuse * diffuse_factor * albedo;
    // Specular.
    const vec3 halfway_dir = normalize(light_dir + view_dir);
    const float specular_factor = pow(max(dot(halfway_dir, normal), 0.0), 32);
    const vec3 specular_color = light.specular * specular_factor * vec3(texture(textures[specular_index], uvs));
    // Attenuation.
    const float distance = length(light.position - frag_pos);
    const float clamped = clamp(1.0 - pow(distance / light.radius, 4), 0, 1);
    const float attenuation = (clamped * clamped) / (1 + distance * distance);
    return (diffuse_color + specular_color) * attenuation;
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

vec3 sample_shadow(vec3 color, vec3 normal, vec3 light_dir, vec3 albedo, vec2 offset, uint layer) {
    const vec4 light_frag_pos = (shadow_bias * cascades[layer].pv) * vec4(frag_pos, 1.0);
    const vec4 shadow_coords = light_frag_pos / light_frag_pos.w;
    const vec2 texel = vec2(shadow_coords.x, 1.0 - shadow_coords.y);
    float bias = 0.00025 * (1 / (cascades[layer].split * 0.65));
    bias = max(bias, bias * (1.0 - dot(normal, light_dir)));
    const float current = shadow_coords.z + bias;
    if (shadow_coords.z > -1.0 && shadow_coords.z < 1.0) {
        const float closest = texture(shadow, vec3(texel + offset, layer)).r;
        if (shadow_coords.w > 0 && current > closest) {
            return ambient_factor * albedo;
        }
    }
    return color;
}


vec3 compute_pcf(DirectionalLight light, vec3 color, vec3 normal, vec3 albedo, uint layer) {
    const ivec2 shadow_size = textureSize(shadow, 0).xy;
    const vec2 texel = 0.33 / shadow_size;
    const vec3 light_dir = normalize(light.direction);
    vec3 shadow = vec3(0.0);
    uint count = 0;
    for (int x = -pcf_range; x <= pcf_range; ++x) {
        for (int y = -pcf_range; y <= pcf_range; ++y) {
            shadow += sample_shadow(color, normal, light_dir, albedo, texel * vec2(x, y), layer);
            count++;
        }
    }
    return shadow / count;
}
