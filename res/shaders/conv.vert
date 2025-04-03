#include <conv>

layout(location = 0) in  vec3 in_pos;
layout(location = 1) in  vec2 in_uv;

layout(location = 0) out vec3 dir;

void main() {
    dir          = in_pos;
    gl_Position  = conv.proj * conv.view * vec4(in_pos, 1.0);
}
