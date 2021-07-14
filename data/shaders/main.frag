#version 460

layout (location = 0) out vec4 pixel;

layout (location = 0) in VertexData {
    vec3 color;
} vin;

void main() {
    pixel = vec4(vin.color, 1.0);
}