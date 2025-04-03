#version 450

layout(set = 0, binding = 0) uniform Basic {
    mat4 proj;
    mat4 model;
    mat4 view;
} basic;

layout(location = 0) in  vec3 in_pos;
layout(location = 1) in  vec2 in_uv;
layout(location = 0) out vec2 uv;

mat4 perspective(float fovY, float aspect, float near, float far) {
    float f = 1.0 / tan(fovY / 2.0);
    float nf = 1.0 / (near - far);

    return mat4(
        f / aspect, 0.0,  0.0,                             0.0,
        0.0,        f,    0.0,                             0.0,
        0.0,        0.0, (far + near) * nf,               -1.0,
        0.0,        0.0, (2.0 * far * near) * nf,          0.0
    );
}

void main() {
    uv = in_uv;
    //mat4 proj = perspective(radians(90.0), 1.0, 0.1, 10.0);
    gl_Position = basic.proj * mat4(1.0) * mat4(1.0) * vec4(in_pos, 1.0);
}