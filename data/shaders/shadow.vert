#version 460

layout (location = 0) in vec3 ivertex;
layout (location = 1) in vec3 inormal;
layout (location = 2) in vec2 iuvs;
layout (location = 3) in vec3 itangents;
layout (location = 4) in vec3 ibitangents;

layout (set = 0, binding = 0) uniform Camera {
    mat4 proj_view;
};

layout (set = 0, binding = 1) buffer readonly Models {
    mat4[] model;
};

layout (push_constant) uniform Constants {
    uint model_index;
};

void main() {
    gl_Position = proj_view * model[model_index] * vec4(ivertex, 1.0);
}
