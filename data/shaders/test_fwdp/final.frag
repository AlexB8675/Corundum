#version 460
#extension GL_ARB_separate_shader_objects: enable
#extension GL_EXT_nonuniform_qualifier: enable

#define tile_size 16
#define max_lights_per_tile 16

#define ambient_factor 0.02

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

layout (set = 1, binding = 1) buffer readonly LightVisibilities {
    LightVisibility[] light_visibilities;
};

layout (push_constant) uniform Indices {
    uint model_index;
    uint diffuse_index;
    uint normal_index;
    uint specular_index;
    uint point_lights_count;
    ivec2 tiles;
};

vec3 fetch_normal_map();
vec3 calculate_point_light(PointLight light, vec3 albedo, vec3 normal, vec3 view_dir);

void main() {
    const vec3 normal = normalize(normal_index != 0 ? fetch_normal_map() : i_normal);
    const vec3 view_dir = normalize(view_pos - frag_pos);
    const vec3 albedo = vec3(texture(textures[diffuse_index], uvs));
    const ivec2 tile_id = ivec2(gl_FragCoord.xy / tile_size);
    const uint tile_index = tile_id.y * tiles.x + tile_id.x;
    vec3 color = albedo * ambient_factor;
    for (uint i = 0; i < light_visibilities[tile_index].count; ++i) {
        color += calculate_point_light(point_lights[light_visibilities[tile_index].indices[i]], albedo, normal, view_dir);
    }
    const float intensity = float(light_visibilities[tile_index].count) / max_lights_per_tile;
    //pixel = vec4(vec3(intensity), 1.0);
    pixel = vec4(color, 1.0);
}

vec3 fetch_normal_map() {
    return TBN * (2.0 * texture(textures[normal_index], uvs).rgb - 1.0);
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