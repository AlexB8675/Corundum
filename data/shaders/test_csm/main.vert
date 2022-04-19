#version 460
#extension GL_ARB_separate_shader_objects: enable

void main() {
    const vec2 uvs = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uvs * 2.0 - 1.0, 0.0, 1.0);
}
