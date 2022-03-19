#version 460
#extension GL_EXT_ray_tracing: enable
#extension GL_EXT_buffer_reference: enable
#extension GL_EXT_scalar_block_layout: enable
#extension GL_EXT_nonuniform_qualifier: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64: enable

struct RayPayload {
    vec3 ray_xdir;
    vec3 ray_ydir;
    vec3 color;
};

layout (location = 0) rayPayloadInEXT RayPayload payload;

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

/*void coordinate_system(vec3 v, out vec3 v1, out vec3 v2) {
    v1 = normalize(abs(v.x) > abs(v.y) ?
        vec3(-v.z, 0, v.x) :
        vec3(0, -v.z, v.y));
    v2 = cross(v, v1);
}

float compute_tmip(vec3 vertices, vec3 ray_xdir, vec3 ray_ydir, uint levels) {
    vec3 face_normal = normalize(cross(v[1].vertex - v[0].vertex, v[2].vertex - v[0].vertex));

    vec3 dpdu, dpdv;
    {
        const vec3 p10 = v[1].vertex - v[0].vertex;
        const vec3 p20 = v[2].vertex - v[0].vertex;

        const float a00 = v[1].uv.x - v[0].uv.x;
        const float a01 = v[1].uv.y - v[0].uv.y;
        const float a10 = v[2].uv.x - v[0].uv.x;
        const float a11 = v[2].uv.y - v[0].uv.y;

        const float det = a00 * a11 - a01 * a10;
        if (abs(det) < 1e-10) {
            coordinate_system(face_normal, dpdu, dpdv);
        } else {
            const float inv_det = 1.0/det;
            dpdu = ( a11 * p10 - a01 * p20) * inv_det;
            dpdv = (-a10 * p10 + a00 * p20) * inv_det;
        }
    }

    vec3 dpdx, dpdy;
    {
        const vec3 point = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
        comst float plane_d = -dot(face_normal, point);

        const float tx = ray_plane_intersection(gl_WorldRayOriginEXT, rx_dir, face_normal, plane_d);
        const float ty = ray_plane_intersection(gl_WorldRayOriginEXT, ry_dir, face_normal, plane_d);

        const vec3 px = gl_WorldRayOriginEXT + rx_dir * tx;
        const vec3 py = gl_WorldRayOriginEXT + ry_dir * ty;

        dpdx = px - point;
        dpdy = py - point;
    }

    float dudx, dvdx, dudy, dvdy;
    {
        uint dim0 = 0, dim1 = 1;
        vec3 a_n = abs(face_normal);
        if (a_n.x > a_n.y && a_n.x > a_n.z) {
            dim0 = 1;
            dim1 = 2;
        } else if (a_n.y > a_n.z) {
            dim0 = 0;
            dim1 = 2;
        }

        const float a00 = dpdu[dim0];
        const float a01 = dpdv[dim0];
        const float a10 = dpdu[dim1];
        const float a11 = dpdv[dim1];

        const float det = a00 * a11 - a01 * a10;
        if (abs(det) < 1e-10) {
            dudx = dvdx = dudy = dvdy = 0;
        } else {
            const float inv_det = 1.0/det;
            dudx = ( a11 * dpdx[dim0] - a01 * dpdx[dim1]) * inv_det;
            dvdx = (-a10 * dpdx[dim0] - a00 * dpdx[dim1]) * inv_det;
            dudy = ( a11 * dpdy[dim0] - a01 * dpdy[dim1]) * inv_det;
            dvdy = (-a10 * dpdy[dim0] - a00 * dpdy[dim1]) * inv_det;
        }
    }
    return levels - 1 + log2(clamp(max(length(vec2(dudx, dvdx)), length(vec2(dudy, dvdy))), 1e-6, 1.0));
}*/

void main() {
    const Object indices = objects[gl_InstanceCustomIndexEXT];
    Vertices v_ref = Vertices(indices.vertex_address);
    Indices i_ref = Indices(indices.index_address);
    const ivec3 index = i_ref.indices[gl_PrimitiveID];
    const Vertex[] vertices = Vertex[3](
        v_ref.vertices[index.x],
        v_ref.vertices[index.y],
        v_ref.vertices[index.z]);
    Vertex[] verts_world = vertices;
    verts_world[0].vertex = gl_ObjectToWorldEXT * vec4(verts_world[0].vertex, 1.0);
    verts_world[1].vertex = gl_ObjectToWorldEXT * vec4(verts_world[1].vertex, 1.0);
    verts_world[2].vertex = gl_ObjectToWorldEXT * vec4(verts_world[2].vertex, 1.0);
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
    vec3 T = normalize(vec3(mat4(gl_ObjectToWorld3x4EXT) * vec4(tangent, 0.0)));
    const vec3 N = normalize(vec3(mat4(gl_ObjectToWorld3x4EXT) * vec4(normal, 0.0)));
    T = normalize(T - dot(T, N) * N);
    const vec3 B = cross(N, T);
    const mat3 TBN = mat3(T, B, N);
    const vec3 albedo = texture(textures[indices.diffuse_index], uvs).rgb;
    vec3 normals = vec3(0.0);
    if (indices.normal_index != 0) {
        normals = normalize(TBN * (2.0 * texture(textures[indices.normal_index], uvs).rgb - 1.0));
    } else {
        normals = inverse(mat3(gl_ObjectToWorld3x4EXT)) * normal;
    }
    payload.color = albedo;
}