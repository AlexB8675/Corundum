#version 460
#define ambient_factor 0.15

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

vec3 calculate_point_light(PointLight, vec3, vec3, vec3);

void main() {
    const vec3 frag_pos = subpassLoad(i_position).rgb;
    const vec3 normal = subpassLoad(i_normal).rgb;
    const vec3 view_dir = normalize(view_pos - frag_pos);

    vec3 color = subpassLoad(i_albedo).rgb * ambient_factor;
    for (uint i = 0; i < point_lights.length(); ++i) {
        color += calculate_point_light(point_lights[i], normal, frag_pos, view_dir);
    }
    pixel = vec4(color, 1.0);
}

vec3 calculate_point_light(PointLight light, vec3 normal, vec3 frag_pos, vec3 view_dir) {
    const vec3 light_dir = normalize(light.position - frag_pos);
    // diffuse shading.
    const float diffuse_component = max(dot(normal, light_dir), 0.0);
    // specular shading.
    const float specular_component = pow(max(dot(view_dir, reflect(-light_dir, normal)), 0.0), 32);
    // attenuation.
    const float distance = length(light.position - frag_pos);
    const float attenuation = 1.0 / (light.falloff.x + light.falloff.y * distance + light.falloff.z * (distance * distance));
    // combine.
    vec3 color = ambient_factor * subpassLoad(i_albedo).rgb;
    vec3 diffuse = light.diffuse * diffuse_component * subpassLoad(i_albedo).rgb;
    vec3 specular = light.specular * specular_component * vec3(subpassLoad(i_specular).r);
    color *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return color + diffuse + specular;
}
