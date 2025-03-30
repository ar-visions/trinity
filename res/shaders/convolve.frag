#include <env>

layout(set = 0, binding = 8) uniform samplerCube tx_environment;

layout(location = 0) in vec3 dir;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
vec2 hammersley2d(uint i, uint N) {
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float rdi = float(bits) * 2.3283064365386963e-10;
    return vec2(float(i) / float(N), rdi);
}

vec3 sampleHemisphere(vec2 Xi, vec3 center, float roughness) {
    //float thetaMax = roughness * PI * 0.5; // max cone angle
    float thetaMax = min(roughness * PI * 0.5, radians(75.0));
    float cosTheta = mix(1.0, cos(thetaMax), Xi.y);
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float phi      = 2.0 * PI * Xi.x;
    vec3  local    = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // align to 'center' direction
    vec3  up       = abs(center.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3  tangentX = normalize(cross(up, center));
    vec3  tangentY = cross(center, tangentX);
    return normalize(local.x * tangentX + local.y * tangentY + local.z * center);
}

vec4 prefilterEnvMap(vec3 R, float roughness, int numSamples) {
    vec3  color         = vec3(0.0);
    float totalWeight   = 0.0;
    float envMapDim     = float(textureSize(tx_environment, 0).s);
    float sigma         = roughness * PI * 0.5;

    for (uint i = 0u; i < uint(numSamples); ++i) {
        vec2  Xi        = hammersley2d(i, uint(numSamples));
        vec3  rand      = sampleHemisphere(Xi, dir, roughness); // <--- your actual random sample direction around R
        float dotpr     = clamp(dot(dir, rand), 0.0, 1.0); // angular closeness to reflection direction
        float theta     = acos(dotpr); // deviation angle
        float w         = exp(-theta * theta / (2.0 * sigma * sigma)); // Gaussian weight
        
        if (dotpr > 0.0) {
            vec3  s      = textureLod(tx_environment, rand, 0).rgb;
            color       += s * w;
            totalWeight +=     w;
        }
    }

    return vec4(color / totalWeight, 1.0);
}

void main() {
    float roughness  = e.roughness_samples.x;
    int   numSamples = int(e.roughness_samples.y);
    outColor = (roughness < 0.01) ? texture(tx_environment, normalize(dir)) : 
        prefilterEnvMap(normalize(dir), roughness, numSamples);
}