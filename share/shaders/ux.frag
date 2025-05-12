#include <ux>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

void main() {
    fragColor = texture(tx_canvas, v_uv);
}
