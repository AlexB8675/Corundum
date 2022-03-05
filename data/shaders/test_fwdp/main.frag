#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_separate_shader_objects: enable

#define ambient_factor 0.03
#define max_lights_per_tile 16
#define max_shadow_cascades 16
#define max_directional_lights 4
#define shadow_cascades 4
#define pcf_range 6

layout (location = 0) out vec4 pixel;

const mat4 shadow_bias = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0);

struct Cascade {
    mat4 pv;
    float split;
};

struct LightVisibility {
    uint count;
    uint[max_lights_per_tile] indices;
};

struct DirectionalLight {
    vec3 direction;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;
    float _pad0;
    vec3 diffuse;
    float _pad1;
    vec3 specular;
    float _pad2;
    float radius;
    vec3 _pad3;
};

layout (location = 0) in VertexData {
    mat3 TBN;
    vec3 frag_pos;
    vec3 view_pos;
    vec3 normal;
    vec2 uvs;
};

layout (set = 0, binding = 2) uniform DirectionalLights {
    DirectionalLight[max_directional_lights] directional_lights;
};

layout (std430, set = 0, binding = 3) buffer readonly PointLights {
    PointLight[] point_lights;
};

layout (set = 0, binding = 4) uniform sampler2DArray shadow;

layout (set = 0, binding = 5) uniform Cascades {
    Cascade cascades[max_shadow_cascades];
};

layout (set = 0, binding = 6) buffer readonly LightVisibilities {
    LightVisibility[] plights_visible;
};

layout (set = 0, binding = 7) uniform sampler2D[] textures;

layout (push_constant) uniform Constants {
    uint model_index;
    uint diffuse_index;
    uint normal_index;
    uint specular_index;
    uint point_light_size;
    uint directional_light_size;
};

vec3 calculate_directional_light(DirectionalLight light, vec3 color, vec3 normal, vec3 view_dir);
vec3 calculate_point_light(PointLight light, vec3 color, vec3 normal, vec3 frag_pos, vec3 view_dir);
vec3 calculate_vsm_shadow(vec3 color, vec3 normal, vec3 light_dir, vec3 frag_pos, vec3 albedo, uint layer);
vec3 calculate_shadow(vec3 color, vec3 normal, vec3 light_dir, vec3 frag_pos, vec3 albedo, vec2 offset, uint layer);
vec3 filter_pcf(DirectionalLight light, vec3 color, vec3 normal, vec3 frag_pos, vec3 albedo, uint layer);
uint calculate_layer(float view_depth);

void main() {
    const vec3 normal = normalize(normal_index != 0 ? (TBN * (2.0 * texture(textures[normal_index], uvs).rgb - 1.0)) : normal);
    const vec3 albedo = texture(textures[diffuse_index], uvs).rgb;
    const vec3 view_dir = normalize(view_pos - frag_pos);

    vec3 color = albedo * ambient_factor;
    //const uint layer = calculate_layer(view_depth);
    /*for (uint i = 0; i < directional_light_size; ++i) {
        color += calculate_directional_light(directional_lights[i], albedo, normal, view_dir);
        //color = filter_pcf(directional_lights[i], color, normal, frag_pos, albedo, layer);
    }*/
    for (uint i = 0; i < point_light_size; ++i) {
        color += calculate_point_light(point_lights[i], albedo, normal, frag_pos, view_dir);
    }
    pixel = vec4(color, 1.0);
}

vec3 calculate_directional_light(DirectionalLight light, vec3 color, vec3 normal, vec3 view_dir) {
    const vec3 light_dir = normalize(light.direction);
    // Diffuse.
    const float diffuse_comp = max(dot(light_dir, normal), 0.0);
    // Specular.
    const vec3 halfway_dir = normalize(light_dir + view_dir);
    const float specular_comp = pow(max(dot(halfway_dir, normal), 0.0), 32);

    // Combine.
    const vec3 result_diffuse = vec3(light.diffuse) * diffuse_comp * color;
    const vec3 result_specular = vec3(light.specular) * specular_comp * texture(textures[specular_index], uvs).rgb;
    return result_diffuse + result_specular;
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
    const float attenuation = light.radius / (distance * distance);

    // Combine.
    const vec3 result_diffuse = vec3(light.diffuse) * diffuse_comp * color;
    const vec3 result_specular = vec3(light.specular) * specular_comp * texture(textures[specular_index], uvs).rgb;
    return (result_diffuse + result_specular) * attenuation;
}

uint calculate_layer(float view_depth) {
    uint layer = shadow_cascades - 1;
    for (uint i = 0; i < shadow_cascades; ++i) {
        if (abs(view_depth) < cascades[i].split) {
            layer = i;
            break;
        }
    }
    return layer;
}

vec3 calculate_shadow(vec3 color, vec3 normal, vec3 light_dir, vec3 frag_pos, vec3 albedo, vec2 offset, uint layer) {
    const vec4 light_frag_pos = (shadow_bias * cascades[layer].pv) * vec4(frag_pos, 1.0);
    const vec4 shadow_coords = light_frag_pos / light_frag_pos.w;
    const vec2 texel = vec2(shadow_coords.x, 1.0 - shadow_coords.y);
    float bias = 0.00005 * (1 / (cascades[layer].split * 0.5));
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


vec3 filter_pcf(DirectionalLight light, vec3 color, vec3 normal, vec3 frag_pos, vec3 albedo, uint layer) {
    const ivec2 shadow_size = textureSize(shadow, 0).xy;
    const vec2 texel = 0.25 / shadow_size;
    const vec3 light_dir = normalize(light.direction);
    vec3 shadow = vec3(0.0);
    uint count = 0;
    for (int x = -pcf_range; x <= pcf_range; ++x) {
        for (int y = -pcf_range; y <= pcf_range; ++y) {
            shadow += calculate_shadow(color, normal, light_dir, frag_pos, albedo, texel * vec2(x, y), layer);
            count++;
        }
    }
    return shadow / count;
}