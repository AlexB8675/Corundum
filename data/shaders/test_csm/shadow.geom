#version 460

#define max_shadow_cascades 16
#define shadow_cascades 4

layout (triangles, invocations = shadow_cascades) in;
layout (triangle_strip, max_vertices = 3) out;

layout (location = 0) in vec2[] i_uvs;

layout (location = 0) out vec2 uvs;

struct Cascade {
    mat4 pv;
    float split;
};

layout (set = 0, binding = 1) uniform Cascades {
    Cascade cascades[max_shadow_cascades];
};

void main() {
    for (int i = 0; i < 3; ++i) {
        gl_Position = cascades[gl_InvocationID].pv * gl_in[i].gl_Position;
        uvs = i_uvs[gl_InvocationID];
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}
