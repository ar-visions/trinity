#include <simple>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

void main() {
    vec4 background = texture(tx_background, v_uv);
    vec4 overlay    = texture(tx_overlay, v_uv);
    vec3 colorized  = mix(background.xyz, overlay.xyz, overlay.a);
    fragColor       = mix(colorized, 1.0);
}
