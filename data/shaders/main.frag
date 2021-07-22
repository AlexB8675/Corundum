#version 460

layout (location = 0) out vec4 pixel;

layout (location = 0) in VertexData {
    vec2 uvs;
};

layout (set = 0, binding = 1) uniform sampler2D image;

void main() {
    pixel = vec4(texture(image, uvs).rgb, 1.0);
}
