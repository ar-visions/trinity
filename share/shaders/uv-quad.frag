#include <uv-quad>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

void main() {
    // find out where the texture is being bound to uniform; this is the descriptor binding for this uniform sampler2D
    // lets get the return of this, track it for this specific shader
    // * ux-quad
    //      * as provided in swap-level render pass

    //float   a = dot(texture(tx_color, v_uv).xyz, vec3(1.0 / 3.0));
    //fragColor = vec4(a, 0.4, 1.0, 1.0);
    fragColor = vec4(texture(tx_color, v_uv).xyz, 1.0);
}
