#include <import>

int main(int argc, char *argv[]) {
    A_start();
    trinity t = trinity();
    window  w = window(t, t, shader, null, width, 800, height, 600);
    loop   (w);
    return  0;
}