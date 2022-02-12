#version 460
#extension GL_EXT_nonuniform_qualifier: enable

layout (set = 0, binding = 2) uniform sampler2D[] textures;

layout (location = 0) in vec2 uvs;

layout (push_constant) uniform Constants {
    uint model_index;
    uint diffuse_index;
};

void main() {
    // TODO: Alpha discard?
}
