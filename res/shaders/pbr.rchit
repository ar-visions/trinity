#version 460
#extension GL_EXT_ray_tracing : enable

#include <import>

layout(location = 0) rayPayloadInEXT vec3 payloadColor;

hitAttributeEXT vec2 hitTexCoord;

void main() {
    vec3  albedo   = texture(color, hitTexCoord).rgb;
    float metallic = texture(metal, hitTexCoord).x;
    float roughess = texture(rough, hitTexCoord).x;
    vec3  normal   = normalize(texture(normal, hitTexCoord).rgb * 2.0 - 1.0);
    
    // Simple shading (we'll replace this with proper lighting below)
    vec3 viewDir = normalize(-gl_WorldRayDirectionEXT);
    vec3 lightDir = normalize(vec3(1.0, -1.0, -1.0)); // Fake directional light

    float ndotl = max(dot(normal, lightDir), 0.0);
    payloadColor = albedo * ndotl;
}
