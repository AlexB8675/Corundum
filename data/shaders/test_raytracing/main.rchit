#version 460
#extension GL_EXT_ray_tracing: enable
#extension GL_EXT_buffer_reference: enable
#extension GL_EXT_scalar_block_layout: enable
#extension GL_EXT_nonuniform_qualifier: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64: enable

layout (location = 0) rayPayloadInEXT vec3 hit;

struct Vertex {
    vec3 vertex;
    vec3 normal;
    vec2 uv;
    vec3 tangent;
    vec3 bitangent;
};

struct Object {
    uint diffuse_index;
    uint normal_index;
    uint specular_index;
    uint64_t vertex_address;
    uint64_t index_address;
};

struct Instance {
    mat3x4 transform;
    uint[6] _pad0;
};

layout (buffer_reference, scalar) readonly buffer Vertices {
    Vertex[] vertices;
};

layout (buffer_reference, scalar) readonly buffer Indices {
    ivec3[] indices;
};

layout (set = 0, binding = 3, scalar) readonly buffer Objects {
    Object[] objects;
};

layout (set = 0, binding = 4) uniform sampler2D[] textures;

hitAttributeEXT vec2 attribs;

void main() {
    const Object indices = objects[gl_InstanceCustomIndexEXT];
    Vertices v_ref = Vertices(indices.vertex_address);
    Indices i_ref = Indices(indices.index_address);
    const ivec3 index = i_ref.indices[gl_PrimitiveID];
    const Vertex[] vertices = Vertex[3](
        v_ref.vertices[index.x],
        v_ref.vertices[index.y],
        v_ref.vertices[index.z]);
    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.xy);
    const vec3 vertex =
        vertices[0].vertex * barycentrics.x +
        vertices[1].vertex * barycentrics.y +
        vertices[2].vertex * barycentrics.z;
    const vec3 normal =
        vertices[0].normal * barycentrics.x +
        vertices[1].normal * barycentrics.y +
        vertices[2].normal * barycentrics.z;
    const vec2 uvs =
        vertices[0].uv * barycentrics.x +
        vertices[1].uv * barycentrics.y +
        vertices[2].uv * barycentrics.z;
    const vec3 tangent =
        vertices[0].tangent * barycentrics.x +
        vertices[1].tangent * barycentrics.y +
        vertices[2].tangent * barycentrics.z;
    vec3 T = normalize(vec3(gl_ObjectToWorldEXT * vec4(tangent, 0.0)));
    const vec3 N = normalize(vec3(gl_ObjectToWorldEXT * vec4(normal, 0.0)));
    T = normalize(T - dot(T, N) * N);
    const vec3 B = cross(N, T);
    const mat3 TBN = mat3(T, B, N);
    const vec3 albedo = texture(textures[indices.diffuse_index], uvs).rgb;
    vec3 normals = vec3(0.0);
    if (indices.normal_index != 0) {
        normals = normalize(TBN * (2.0 * texture(textures[indices.normal_index], uvs).rgb - 1.0));
    } else {
        normals = mat3(gl_ObjectToWorld3x4EXT) * normal;
    }
    hit = normals;
}