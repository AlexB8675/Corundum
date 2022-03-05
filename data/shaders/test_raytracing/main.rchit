#version 460
#extension GL_EXT_ray_tracing: enable

layout (location = 0) rayPayloadInEXT vec3 hit;

hitAttributeEXT vec2 attribs;

void main() {
    hit = vec3(1.0 - attribs.x - attribs.y, attribs.xy);
}