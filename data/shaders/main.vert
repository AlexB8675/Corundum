#version 460

layout (location = 0) in vec3 ivertex;
layout (location = 1) in vec3 inormal;
layout (location = 2) in vec2 iuvs;
layout (location = 3) in vec3 itangents;
layout (location = 4) in vec3 ibitangents;

layout (location = 0) out VertexData {
    vec2 uvs;
};

layout (set = 0, binding = 0) uniform Camera {
    mat4 proj_view;
};

void main() {
    gl_Position = proj_view * vec4(ivertex, 1.0);
    uvs = iuvs;
}
