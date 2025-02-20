#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec3 payloadColor;
layout(set = 0, binding = 1) uniform sampler2D environmentMap;

void main() {
    vec3 envColor = texture(environmentMap, normalize(gl_WorldRayDirectionEXT).xy * 0.5 + 0.5).rgb;
    payloadColor = envColor;
}
