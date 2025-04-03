#include <pbr>

layout(location = 0) out vec4 fragColor;
layout(location = 1) in  vec3 v_world_normal;
layout(location = 2) in  vec3 v_world_pos;
layout(location = 3) in  vec3 v_view_pos;
layout(location = 4) in  vec2 v_uv;

// PBR functions based on Disney standard
// Constants
const float PI = 3.14159265359;
const float EPSILON = 0.0001;

// Helper functions
float pow2(float x) { return x * x; }
float pow5(float x) { float x2 = x * x; return x2 * x2 * x; }

// Proper gamma handling
vec3 sRGBToLinear(vec3 srgbColor) {
    return pow(srgbColor, vec3(2.2));
}

vec3 linearToSRGB(vec3 srgbColor) {
    return pow(srgbColor, vec3(1.0/2.2));
}

// Fresnel Schlick approximation
vec3 F_Schlick(vec3 f0, float cos_theta) {
    return f0 + (1.0 - f0) * pow5(1.0 - cos_theta);
}

// Normal Distribution Function: GGX/Trowbridge-Reitz
float D_GGX(float roughness, float NdotH) {
    float alpha = pow2(roughness);
    float alpha_sq = pow2(alpha);
    float denom = pow2(NdotH) * (alpha_sq - 1.0) + 1.0;
    return alpha_sq / (PI * pow2(denom));
}

// Geometry Function: Smith with GGX
float G_Smith(float roughness, float NdotV, float NdotL) {
    float k = pow2(roughness + 1.0) / 8.0;
    float G1_V = NdotV / max((NdotV * (1.0 - k) + k), 0.0001);
    float G1_L = NdotL / max((NdotL * (1.0 - k) + k), 0.0001);
    return G1_V * G1_L;
}


// Then modify the BRDF function to use these adjustments:
vec3 BRDF(vec3 L, vec3 V, vec3 N, vec3 albedo, float metallic, float roughness) {
    // Clamp roughness to avoid divide by zero in D_GGX
    roughness = max(roughness, 0.05);
    
    vec3 H = normalize(L + V);
    
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    
    // F0 represents the base reflectivity
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // Calculate Cook-Torrance components
    vec3 F = F_Schlick(F0, HdotV);
    float D = D_GGX(roughness, NdotH);
    float G = G_Smith(roughness, NdotV, NdotL);
    
    // Specular term
    vec3 specular = (D * F * G) / max(4.0 * NdotV * NdotL, EPSILON);
    
    // Diffuse term - metallic surfaces have no diffuse
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;
    
    return (diffuse + specular) * NdotL;
}

// Normal mapping function
vec3 perturbNormal(vec3 N, vec3 V, vec2 texcoord) {
    vec3 map = texture(tx_normal, texcoord).rgb * 2.0 - 1.0;
    
    // Calculate tangent space
    vec3 q1 = dFdx(v_world_pos);
    vec3 q2 = dFdy(v_world_pos);
    vec2 st1 = dFdx(texcoord);
    vec2 st2 = dFdy(texcoord);
    
    vec3 T = normalize(q1 * st2.y - q2 * st1.y);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    
    return normalize(TBN * map);
}

// Parallax mapping
vec2 parallaxMapping(vec2 texCoords, vec3 viewDir) {
    float height_scale = 0.04;
    float height = texture(tx_height, texCoords).r;
    vec2 p = viewDir.xy / viewDir.z * (height * height_scale);
    return texCoords - p;
}

vec3 environmentMapping(vec3 R, float roughness) {
    float lod   = roughness * 7.0; // Adjust based on your mipmap count
    float lodf  = floor(lod);
    float lodfr = lod - lodf;
    int   mip0  = int(lodf);
    int   mip1  = min(mip0 + 1, 7); 
    vec3  s0    = texture(tx_environment, vec4(R, mip0)).rgb;
    vec3  s1    = texture(tx_environment, vec4(R, mip1)).rgb;
    return mix(s0, s1, lodfr);
}

// Main PBR function
vec4 calculatePBR(vec2 texCoords, vec3 worldPos, vec3 normal, vec3 viewPos) {
    // Get camera position
    vec3 cameraPos = world.pos.xyz;
    
    // View direction
    vec3 V = normalize(cameraPos - worldPos);
    
    // Apply parallax mapping for texture coordinate
    vec2 texCoordsMapped = parallaxMapping(texCoords, V);
    
    // Get material properties
    vec4  baseColor     = texture(tx_color, texCoordsMapped);
    float metallic      = texture(tx_metal, texCoordsMapped).r;
    float roughness     = texture(tx_rough, texCoordsMapped).r;
    float ao            = texture(tx_ao, texCoordsMapped).r;
    vec3  emission      = texture(tx_emission, texCoordsMapped).rgb;
    float intensity     = texture(tx_emission, texCoordsMapped).a;
    
    vec3 rough_color = baseColor.rgb; // energyCompensation(baseColor.rgb, roughness);

    // Apply normal mapping
    vec3 N = perturbNormal(normal, V, texCoordsMapped);
    
    // Reflection vector for environment mapping
    vec3 R = reflect(-V, N);
    
    // Environment lighting
    vec3 envColor = environmentMapping(R, roughness);
    
    // Ambient light
    vec3 ambient = envColor * rough_color * ao;
    
    // Directional light (example light, replace with your light source)
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 lightColor = vec3(1.0);
    
    // Calculate BRDF
    vec3 Lo = BRDF(lightDir, V, N, rough_color, metallic, roughness) * lightColor;
    
    // Combine direct and indirect lighting
    vec3 color = ambient + Lo + emission * intensity;
    
    // Convert from linear to sRGB for display
    color = linearToSRGB(color);
    
    return vec4(color, baseColor.a);
}


void main() {
    // Calculate PBR lighting
    fragColor = calculatePBR(v_uv, v_world_pos, normalize(v_world_normal), v_view_pos);
    //fragColor = vec4(vec3(1.0), 1.0);
}
