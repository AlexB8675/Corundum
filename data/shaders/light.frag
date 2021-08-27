#version 460
#extension GL_EXT_nonuniform_qualifier: enable

layout (location = 0) out vec4 pixel;

layout (set = 0, binding = 2) buffer readonly Colors {
    vec4[] color;
};

layout (push_constant) uniform Constants {
    uint model_index;
};

void main() {
    pixel = vec4(vec3(color[model_index]), 1.0);
}
