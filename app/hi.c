#include <import>

int main(int argc, char *argv[]) {
    A_start();
    trinity t = trinity();
    shader  s = shader(t, t, name, str("shader.wgsl"));
    window  w = window(t, t, shader, s, width, 800, height, 600);
    loop   (w);
    return  0;
}