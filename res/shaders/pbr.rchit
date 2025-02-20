#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec3 payloadColor;


layout(set = 0, binding = 0) uniform PBR {







// Core PBR Properties
    vec4  baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    vec3  emissiveFactor;
    float emissiveStrength;

    // Specular Glossiness (KHR_materials_pbrSpecularGlossiness)
    vec4  diffuseFactor;
    vec3  specularFactor;
    float glossinessFactor;

    // Sheen (KHR_materials_sheen)
    vec3  sheenColorFactor;
    float sheenRoughnessFactor;

    // Clear Coat (KHR_materials_clearcoat)
    float clearcoatFactor;
    float clearcoatRoughnessFactor;

    // Transmission (KHR_materials_transmission)
    float transmissionFactor;

    // Volume (KHR_materials_volume)
    float thicknessFactor;
    vec3  attenuationColor;
    float attenuationDistance;

    // Index of Refraction (KHR_materials_ior)
    float ior;

    // Specular (KHR_materials_specular)
    vec3  specularColorFactor;

    // Iridescence (KHR_materials_iridescence)
    float iridescenceFactor;
    float iridescenceIor;
    float iridescenceThicknessMinimum;
    float iridescenceThicknessMaximum;
} pbr;

layout(set = 0, binding = 1) uniform sampler2D baseColorTexture;
layout(set = 0, binding = 2) uniform sampler2D metallicRoughnessTexture;
layout(set = 0, binding = 3) uniform sampler2D normalTexture;

hitAttributeEXT vec2 hitTexCoord;

void main() {
    vec3 albedo = texture(baseColorTexture, hitTexCoord).rgb * pbr.baseColorFactor.rgb;
    vec2 metalRough = texture(metallicRoughnessTexture, hitTexCoord).rg;
    float metallic = metalRough.r * pbr.metallicFactor;
    float roughness = metalRough.g * pbr.roughnessFactor;

    vec3 normal = normalize(texture(normalTexture, hitTexCoord).rgb * 2.0 - 1.0);
    
    // Simple shading (we'll replace this with proper lighting below)
    vec3 viewDir = normalize(-gl_WorldRayDirectionEXT);
    vec3 lightDir = normalize(vec3(1.0, -1.0, -1.0)); // Fake directional light

    float ndotl = max(dot(normal, lightDir), 0.0);
    payloadColor = albedo * ndotl;
}
