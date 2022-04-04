#version 460
#extension GL_EXT_nonuniform_qualifier: enable
#extension GL_ARB_separate_shader_objects: enable

layout (set = 0, binding = 2) uniform sampler2D[] textures;

layout (location = 0) in vec2 uvs;

layout (push_constant) uniform Constants {
    uint model_index;
    uint diffuse_index;
};

void main() {
    // const float depth = gl_FragCoord.z;
    // const float ddx = dFdx(depth);
    // const float ddy = dFdy(depth);
    // const float s_depth = depth * depth + 0.25 * (ddx * ddx + ddy * ddy);
    // pixel = vec2(depth, s_depth);
    if (texture(textures[diffuse_index], uvs).a < 0.85) {
        discard;
    }
}
