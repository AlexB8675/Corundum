#version 460

layout (location = 0) out vec4 pixel;

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput i_position;
layout (input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput i_normal;
layout (input_attachment_index = 0, set = 0, binding = 2) uniform subpassInput i_specular;
layout (input_attachment_index = 0, set = 0, binding = 3) uniform subpassInput i_albedo;

void main() {
    pixel = subpassLoad(i_albedo);
}