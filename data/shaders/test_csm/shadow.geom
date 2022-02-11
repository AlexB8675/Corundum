#version 460

#define max_shadow_cascades 16
#define shadow_cascades 6

layout (triangles, invocations = shadow_cascades) in;
layout (triangle_strip, max_vertices = 3) out;

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
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}
