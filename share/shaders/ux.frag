#include <ux>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

void main() {
    fragColor = vec4(mix(texture(tx_blur, v_uv).xyz, texture(tx_canvas, v_uv).xyz, 0.5), 1.0);
}
