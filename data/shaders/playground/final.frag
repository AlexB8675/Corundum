#version 460
#extension GL_ARB_separate_shader_objects: enable
#extension GL_EXT_nonuniform_qualifier: enable

#define tile_size 16
#define max_lights_per_tile 512
#define max_shadow_cascades 16
#define max_directional_lights 4
#define shadow_cascades 4
#define pcss_blocker_samples 64
#define pcss_pcf_samples 64
#define light_near 2.5
#define light_far 256.0
#define pcss_epsilon 0.01
#define light_size vec2(0.0025)
#define ambient_factor 0.075

const vec2 poisson_disc[] = vec2[](
    vec2(-0.934812, 0.366741),
    vec2(-0.918943, -0.0941496),
    vec2(-0.873226, 0.62389),
    vec2(-0.8352, 0.937803),
    vec2(-0.822138, -0.281655),
    vec2(-0.812983, 0.10416),
    vec2(-0.786126, -0.767632),
    vec2(-0.739494, -0.535813),
    vec2(-0.681692, 0.284707),
    vec2(-0.61742, -0.234535),
    vec2(-0.601184, 0.562426),
    vec2(-0.607105, 0.847591),
    vec2(-0.581835, -0.00485244),
    vec2(-0.554247, -0.771111),
    vec2(-0.483383, -0.976928),
    vec2(-0.476669, -0.395672),
    vec2(-0.439802, 0.362407),
    vec2(-0.409772, -0.175695),
    vec2(-0.367534, 0.102451),
    vec2(-0.35313, 0.58153),
    vec2(-0.341594, -0.737541),
    vec2(-0.275979, 0.981567),
    vec2(-0.230811, 0.305094),
    vec2(-0.221656, 0.751152),
    vec2(-0.214393, -0.0592364),
    vec2(-0.204932, -0.483566),
    vec2(-0.183569, -0.266274),
    vec2(-0.123936, -0.754448),
    vec2(-0.0859096, 0.118625),
    vec2(-0.0610675, 0.460555),
    vec2(-0.0234687, -0.962523),
    vec2(-0.00485244, -0.373394),
    vec2(0.0213324, 0.760247),
    vec2(0.0359813, -0.0834071),
    vec2(0.0877407, -0.730766),
    vec2(0.14597, 0.281045),
    vec2(0.18186, -0.529649),
    vec2(0.188208, -0.289529),
    vec2(0.212928, 0.063509),
    vec2(0.23661, 0.566027),
    vec2(0.266579, 0.867061),
    vec2(0.320597, -0.883358),
    vec2(0.353557, 0.322733),
    vec2(0.404157, -0.651479),
    vec2(0.410443, -0.413068),
    vec2(0.413556, 0.123325),
    vec2(0.46556, -0.176183),
    vec2(0.49266, 0.55388),
    vec2(0.506333, 0.876888),
    vec2(0.535875, -0.885556),
    vec2(0.615894, 0.0703452),
    vec2(0.637135, -0.637623),
    vec2(0.677236, -0.174291),
    vec2(0.67626, 0.7116),
    vec2(0.686331, -0.389935),
    vec2(0.691031, 0.330729),
    vec2(0.715629, 0.999939),
    vec2(0.8493, -0.0485549),
    vec2(0.863582, -0.85229),
    vec2(0.890622, 0.850581),
    vec2(0.898068, 0.633778),
    vec2(0.92053, -0.355693),
    vec2(0.933348, -0.62981),
    vec2(0.95294, 0.156896));

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
    mat4 projection;
    mat4 view;
    mat4 proj_view;
    float near;
    float split;
};

struct LightVisibility {
    uint count;
    uint[max_lights_per_tile] indices;
};

layout (location = 0) in VertexData {
    mat3 TBN;
    vec3 frag_pos;
    vec3 view_pos;
    vec3 i_normal;
    vec2 uvs;
    float view_depth;
    float frustum_size;
};

layout (location = 0) out vec4 pixel;

layout (set = 0, binding = 2) uniform sampler2D[] textures;

/*layout (set = 1, binding = 0) buffer readonly PointLights {
    PointLight[] point_lights;
};*/

layout (set = 1, binding = 1) uniform DirectionalLights {
    DirectionalLight[max_directional_lights] directional_lights;
};

/*layout (set = 1, binding = 2) buffer readonly LightVisibilities {
    LightVisibility[] light_visibilities;
};*/

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
    //ivec2 tiles;
};

vec3 fetch_normal_map();
vec3 calculate_directional_light(DirectionalLight light, vec3 albedo, vec3 normal, vec3 view_dir);
vec3 calculate_point_light(PointLight light, vec3 albedo, vec3 normal, vec3 view_dir);
uint calculate_layer();
float compute_pcss(DirectionalLight light, vec3 normal, uint layer);

void main() {
    const vec3 normal = fetch_normal_map();
    const vec3 view_dir = normalize(view_pos - frag_pos);
    const vec3 albedo = vec3(texture(textures[diffuse_index], uvs));
    //const ivec2 tile_id = ivec2(gl_FragCoord.xy / tile_size);
    //const uint tile_index = tile_id.y * tiles.x + tile_id.x;
    const uint layer = calculate_layer();
    vec3 color = albedo * ambient_factor;
    for (uint i = 0; i < directional_lights_count; ++i) {
        color += calculate_directional_light(directional_lights[i], albedo, normal, view_dir) *
                 compute_pcss(directional_lights[i], normal, layer);
    }
    /*for (uint i = 0; i < light_visibilities[tile_index].count; ++i) {
        color += calculate_point_light(point_lights[light_visibilities[tile_index].indices[i]], albedo, normal, view_dir);
    }*/
    pixel = vec4(color, 1.0);
    //pixel = vec4(vec3(float(light_visibilities[tile_index].count) / max_lights_per_tile), 1.0);
}

vec3 fetch_normal_map() {
    if (normal_index == 0) {
        return normalize(i_normal);
    }
    return normalize(TBN * (2.0 * vec3(texture(textures[normal_index], uvs)) - 1.0));
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

/*vec3 calculate_point_light(PointLight light, vec3 albedo, vec3 normal, vec3 view_dir) {
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
}*/

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

float compute_pcss(DirectionalLight light, vec3 normal, uint layer) {
    const vec4 light_frag_pos = (shadow_bias * cascades[layer].proj_view) * vec4(frag_pos, 1.0);
    const vec4 shadow_coords = light_frag_pos / light_frag_pos.w;
    const vec2 texel = vec2(shadow_coords.x, 1.0 - shadow_coords.y);
    const vec2 t_size = 1.5 / textureSize(shadow, 0).xy;
    const vec3 light_dir = normalize(light.direction);
    const float uv_radius = 1 / frustum_size;
    float bias = 0.00025 * (1 / (cascades[layer].split * 0.5));
    bias = max(bias, bias * (1.0 - dot(normal, light_dir)));
    const float current = shadow_coords.z + bias;

    // calculate search width
    const float search_width = uv_radius * (shadow_coords.z - light_near) / shadow_coords.z;

    // searches for blockers
    float blocker_depth = 0;
    uint blocker_count = 0;
    for (int i = 0; i < pcss_blocker_samples; ++i) {
        const vec2 offset = poisson_disc[i] * search_width * t_size;
        const float closest = texture(shadow, vec3(texel + offset, layer)).x;
        if (current > closest) {
            blocker_depth += closest;
            blocker_count++;
        }
    }

    if (blocker_count == 0) {
        return 1;
    }

    const float average_blocker_depth = blocker_depth / blocker_count;
    const float penumbra_width = (shadow_coords.z - average_blocker_depth) / average_blocker_depth;
    const float filter_radius = penumbra_width * uv_radius * light_near / shadow_coords.z;

    // filtering
    float shadow_factor = 0.0;
    for (int i = 0; i < pcss_pcf_samples; ++i) {
        const vec2 offset = poisson_disc[i] * filter_radius;
        const float closest = texture(shadow, vec3(texel + offset, layer)).r;
        shadow_factor += float(current > closest);
    }
    return 1.0 - shadow_factor / pcss_pcf_samples;
}
