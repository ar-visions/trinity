#include <ux>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

void main() {
    vec4 compose   = texture(tx_compose, v_uv);
    vec4 overlay   = texture(tx_overlay, v_uv);
    vec4 colorize  = texture(tx_colorize, v_uv);

    vec3 base;
    float brighten = compose.b * 2.0;
    if (compose.r < 0.33) {
        base = texture(tx_background, v_uv).rgb * brighten;
    } else if (compose.r < 0.66) {
        base = texture(tx_blur, v_uv).rgb * brighten;
    } else {
        base = texture(tx_frost, v_uv).rgb * brighten;
    }

    /// the following is wrong
    /// we want two modes here.   that is hue shift or colorize; and we blend the two methods using compose.g

    /// this is a simple hue shift and sat/lum shift.. thats what we want for g0
    vec3 base_hsv  = rgb2hsv(base); // already in your utilities
    base_hsv.x     =   mod(clamp(base_hsv.x + colorize.r, 0.0, 1.0), 1.0);     // hue shift
    base_hsv.y     = clamp(base_hsv.y + colorize.g, 0.0, 1.0); // sat scale
    base_hsv.z     = clamp(base_hsv.z + colorize.b, 0.0, 1.0); // lum scale
    vec3 g0        = hsv2rgb(base_hsv);

    float lum          = dot(base.rgb, vec3(0.299, 0.587, 0.114)); // perceptual gray
    vec3  colorize_hsv = vec3(colorize.r, colorize.g, lum + colorize.b); // treat .b as lum shift
    vec3  g1           = hsv2rgb(colorize_hsv);

    vec3 colorized = mix(g0, g1, compose.g);
    vec4 base_out = vec4(colorized, colorize.a);
    fragColor     = mix(base_out, overlay, overlay.a);
}
