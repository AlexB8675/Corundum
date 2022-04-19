#version 460
#extension GL_ARB_separate_shader_objects: enable
#extension GL_EXT_nonuniform_qualifier: enable

#define max_shadow_cascades 16
#define shadow_cascades 4

layout (triangles, invocations = shadow_cascades) in;
layout (triangle_strip, max_vertices = 3) out;

layout (location = 0) in vec2[] i_uvs;

layout (location = 0) out vec2 uvs;

struct Cascade {
    mat4 projection;
    mat4 view;
    mat4 proj_view;
    float near;
    float split;
};

layout (set = 0, binding = 1) uniform Cascades {
    Cascade cascades[max_shadow_cascades];
};

void main() {
    for (int i = 0; i < 3; ++i) {
        gl_Position = cascades[gl_InvocationID].proj_view * gl_in[i].gl_Position;
        uvs = i_uvs[gl_PrimitiveIDIn];
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}
