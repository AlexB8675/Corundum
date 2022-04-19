#version 460
#extension GL_ARB_separate_shader_objects: enable

layout (location = 0) in vec3 i_vertex;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec2 i_uvs;
layout (location = 3) in vec3 i_tangent;
layout (location = 4) in vec3 i_bitangent;

layout (location = 0) out VertexData {
    mat3 TBN;
    vec3 o_frag_pos;
    vec3 view_pos;
    vec3 o_normal;
    vec2 uvs;
    float view_depth;
    float o_frustum_size;
};

struct CameraData {
    mat4 projection;
    mat4 view;
    vec3 position;
    float frustum_size;
};

layout (set = 0, binding = 0) uniform Uniforms {
    CameraData camera;
};

layout (set = 0, binding = 1) readonly buffer Models {
    mat4[] models;
};

layout (push_constant) uniform Indices {
    uint model_index;
    uint diffuse_index;
    uint normal_index;
    uint specular_index;
    uint directional_lights_count;
    ivec2 tiles;
};

void main() {
    const mat4 model = models[model_index + gl_InstanceIndex];
    const mat3 inv_model = mat3(transpose(inverse(model)));
    const vec3 frag_pos = vec3(model * vec4(i_vertex, 1.0));
    vec3 T = normalize(inv_model * i_tangent);
    vec3 B = normalize(inv_model * i_bitangent);
    vec3 N = normalize(inv_model * i_normal);
    //T = normalize(T - dot(T, N) * N);
    //vec3 B = cross(N, T);
    TBN = mat3(T, B, N);
    view_pos = camera.position;
    o_normal = inv_model * i_normal;
    o_frag_pos = frag_pos;
    o_frustum_size = camera.frustum_size;
    uvs = i_uvs;
    view_depth = (camera.view * vec4(frag_pos, 1.0)).z;
    gl_Position = camera.projection * camera.view * vec4(frag_pos, 1.0);
}
