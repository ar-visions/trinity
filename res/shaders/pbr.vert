#version 450
#include <pbr>

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 tangents;

layout(location = 1) out vec3 n_color;

void main() {
    /// world has our mvp in world.model, view, proj
    /// do we multiply these together?
    mat4 m = world.proj * world.view * world.model;

    n_color = normal;
    gl_Position = m * vec4(pos, 1.0);
}