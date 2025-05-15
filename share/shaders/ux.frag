#include <ux>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

void main() {
    fragColor = texture(tx_layers[1], v_uv); //vec4(mix(texture(tx_layers[0], v_uv).xyz, texture(tx_layers[1], v_uv).xyz, 0.5), 1.0);
}
