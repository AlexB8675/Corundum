#version 460
#extension GL_EXT_nonuniform_qualifier : enable

#define ambient_factor 0.03
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

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput i_position;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput i_normal;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput i_specular;
layout (input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput i_albedo;

layout (location = 0) out vec4 pixel;

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
    const float view_depth = subpassLoad(i_position).a;
    const vec3 frag_pos = subpassLoad(i_position).rgb;
    const vec3 normal = subpassLoad(i_normal).rgb;
    const vec4 albedo = subpassLoad(i_albedo);
    const vec3 view_dir = normalize(view_pos - frag_pos);

    vec3 color = vec3(albedo) * ambient_factor;
    const uint layer = calculate_layer(view_depth);
    for (uint i = 0; i < directional_light_size; ++i) {
        color += calculate_directional_light(directional_lights[i], vec3(albedo), normal, view_dir);
        color = filter_pcf(directional_lights[i], color, normal, frag_pos, vec3(albedo), layer);
    }
    for (uint i = 0; i < point_light_size; ++i) {
        color += calculate_point_light(point_lights[i], vec3(albedo), normal, frag_pos, view_dir);
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
    const vec3 result_specular = vec3(light.specular) * specular_comp * subpassLoad(i_specular).rgb;
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
    const float attenuation = 1.0 / (light.falloff.x + (light.falloff.y * distance) + (light.falloff.z * (distance * distance)));

    // Combine.
    const vec3 result_diffuse = vec3(light.diffuse) * diffuse_comp * color;
    const vec3 result_specular = vec3(light.specular) * specular_comp * subpassLoad(i_specular).rgb;
    return (result_diffuse + result_specular) * attenuation;
}

// Broken, dunno
vec3 calculate_vsm_shadow(vec3 color, vec3 normal, vec3 light_dir, vec3 frag_pos, vec3 albedo, uint layer) {
    const vec4 light_frag_pos = (shadow_bias * cascades[layer].pv) * vec4(frag_pos, 1.0);
    const vec4 shadow_coords = light_frag_pos / light_frag_pos.w;
    const vec2 texel = vec2(shadow_coords.x, 1.0 - shadow_coords.y);
    const vec2 moments = texture(shadow, vec3(texel, layer)).xy;
    const float variance = max(moments.y - moments.x * moments.x, 0.00001);
    const float depth = moments.x - shadow_coords.z;
    return vec3(max(variance / (variance + depth * depth), float(shadow_coords.z <= moments.x)));
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

#define pcf_samples 8

vec3 filter_pcf(DirectionalLight light, vec3 color, vec3 normal, vec3 frag_pos, vec3 albedo, uint layer) {
    const ivec2 shadow_size = textureSize(shadow, 0).xy;
    const vec2 texel = 0.5 / shadow_size;
    const vec3 light_dir = normalize(light.direction);
    const int[pcf_samples] range = {
        -4, -3, -2, -1,
         1,  2,  3,  4,
    };
    vec3 shadow = vec3(0.0);
    for (int x = 0; x < pcf_samples; x++) {
        for (int y = 0; y < pcf_samples; y++) {
            shadow += calculate_shadow(color, normal, light_dir, frag_pos, albedo, vec2(range[x], range[y]) * texel, layer);
        }
    }
    return shadow / (pcf_samples * pcf_samples);
}
