#version 460
#extension GL_EXT_ray_tracing: enable

layout (location = 0) rayPayloadInEXT vec3 miss;

void main() {
    miss = vec3(0.0);
}