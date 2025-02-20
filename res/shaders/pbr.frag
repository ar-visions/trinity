#version 450

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

// TextureInfo struct (for accessing textures)
struct TextureInfo {
    sampler2D texture;
    uint      texCoord;
};

// Texture Bindings (Descriptor Set 0)
layout(set = 0, binding = 1)  uniform sampler2D baseColorTexture;
layout(set = 0, binding = 2)  uniform sampler2D metallicRoughnessTexture;
layout(set = 0, binding = 3)  uniform sampler2D normalTexture;
layout(set = 0, binding = 4)  uniform sampler2D occlusionTexture;
layout(set = 0, binding = 5)  uniform sampler2D emissiveTexture;
layout(set = 0, binding = 6)  uniform sampler2D specularGlossinessTexture;
layout(set = 0, binding = 7)  uniform sampler2D sheenColorTexture;
layout(set = 0, binding = 8)  uniform sampler2D sheenRoughnessTexture;
layout(set = 0, binding = 9)  uniform sampler2D clearcoatTexture;
layout(set = 0, binding = 10) uniform sampler2D clearcoatRoughnessTexture;
layout(set = 0, binding = 11) uniform sampler2D clearcoatNormalTexture;
layout(set = 0, binding = 12) uniform sampler2D transmissionTexture;
layout(set = 0, binding = 13) uniform sampler2D thicknessTexture;
layout(set = 0, binding = 14) uniform sampler2D specularTexture;
layout(set = 0, binding = 15) uniform sampler2D specularColorTexture;
layout(set = 0, binding = 16) uniform sampler2D iridescenceTexture;

// Function to sample textures safely
vec4 sampleTexture(sampler2D tex, vec2 uv) {
    return texture(tex, uv);
}