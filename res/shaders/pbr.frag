#version 450
#include <pbr>

layout(location = 0) out vec4 fragColor;
layout(location = 1) in  vec3 n_color;

void main() {
    fragColor = vec4(1.0, 1.0, 1.0, 1.0) * vec4(n_color, 1.0);
}