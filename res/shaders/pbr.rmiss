#version 460
#extension GL_EXT_ray_tracing : enable

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

#include <import>

layout(location = 0) rayPayloadInEXT vec3 payloadColor;

void main() {
    vec3 envColor = texture(environment, normalize(gl_WorldRayDirectionEXT).xy * 0.5 + 0.5).rgb;
    payloadColor = envColor;
}
