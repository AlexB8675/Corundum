#version 460
#extension GL_ARB_separate_shader_objects: enable
#extension GL_EXT_nonuniform_qualifier: enable

layout (location = 0) out vec4 pixel;

layout (location = 0) in VertexData {
    vec2 uvs;
};

layout (set = 0, binding = 2) uniform sampler2D[] textures;

layout (push_constant) uniform Indices {
    uint model_index;
    uint diffuse_index;
    uint normal_index;
    uint specular_index;
};

void main() {
    pixel = vec4(texture(textures[diffuse_index], uvs).rgb, 1.0);
}
