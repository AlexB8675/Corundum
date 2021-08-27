#version 460
#extension GL_EXT_nonuniform_qualifier: enable

layout (location = 0) out vec4 pixel;

layout (location = 0) in flat uint instance;

layout (set = 0, binding = 2) buffer readonly Colors {
    vec4[] color;
};

void main() {
    pixel = vec4(vec3(color[instance]), 1.0);
}
