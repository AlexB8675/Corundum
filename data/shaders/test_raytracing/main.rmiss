#version 460
#extension GL_EXT_ray_tracing: enable

struct RayPayload {
    vec3 ray_xdir;
    vec3 ray_ydir;
    vec3 color;
};

layout (location = 0) rayPayloadInEXT RayPayload payload;

void main() {
    payload.color = vec3(0.0);
}