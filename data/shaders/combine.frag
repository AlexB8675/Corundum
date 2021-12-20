#version 460
#define ambient_factor 0.05

layout (location = 0) out vec4 pixel;

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput i_position;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput i_normal;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput i_specular;
layout (input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput i_albedo;

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

layout (set = 1, binding = 0) uniform Uniforms {
    vec3 view_pos;
};

layout (std430, set = 1, binding = 1) buffer readonly DirectionalLights {
    DirectionalLight[] directional_lights;
};

layout (std430, set = 1, binding = 2) buffer readonly PointLights {
    PointLight[] point_lights;
};

vec3 calculate_point_light(PointLight, vec3, vec3, vec3, vec3);

void main() {
    const vec3 frag_pos = subpassLoad(i_position).rgb;
    const vec3 normal = subpassLoad(i_normal).rgb;
    const vec3 albedo = subpassLoad(i_albedo).rgb;
    const vec3 view_dir = normalize(view_pos - frag_pos);

    vec3 color = albedo * ambient_factor;
    for (uint i = 0; i < point_lights.length(); ++i) {
        color += calculate_point_light(point_lights[i], albedo, normal, frag_pos, view_dir);
    }
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
    const vec3 result_specular = vec3(light.specular) * specular_comp * (subpassLoad(i_specular).rgb * color);

    return (result_diffuse + result_specular) * attenuation;
}
