#version 450

layout(set = 1, binding = 0) uniform sampler2D tx_environment;

layout(location = 0) out vec4 outColor;
layout(location = 0) in  vec2 uv;

void main() {
    outColor = vec4(texture(tx_environment, uv).xyz, 1.0);
}