
#include <import>

double sqrt(double);
static int n_parts = 24;

int main(int argc, char *argv[]) {
    A_start();

    /// vector allocation of particles (just 256 for now)
    vertex parts = A_alloc(typeid(vertex), n_parts, true);

    // Bottom face edges
    parts[0] = (struct vertex) { .pos = { -0.5, -0.5, -0.5 } };
    parts[1] = (struct vertex) { .pos = { -0.5, -0.5,  0.5 } };
    parts[2] = (struct vertex) { .pos = { -0.5, -0.5,  0.5 } };
    parts[3] = (struct vertex) { .pos = {  0.5, -0.5,  0.5 } };
    parts[4] = (struct vertex) { .pos = {  0.5, -0.5,  0.5 } };
    parts[5] = (struct vertex) { .pos = {  0.5, -0.5, -0.5 } };
    parts[6] = (struct vertex) { .pos = {  0.5, -0.5, -0.5 } };
    parts[7] = (struct vertex) { .pos = { -0.5, -0.5, -0.5 } };

    // Top face edges
    parts[8]  = (struct vertex) { .pos = { -0.5,  0.5, -0.5 } };
    parts[9]  = (struct vertex) { .pos = { -0.5,  0.5,  0.5 } };
    parts[10] = (struct vertex) { .pos = { -0.5,  0.5,  0.5 } };
    parts[11] = (struct vertex) { .pos = {  0.5,  0.5,  0.5 } };
    parts[12] = (struct vertex) { .pos = {  0.5,  0.5,  0.5 } };
    parts[13] = (struct vertex) { .pos = {  0.5,  0.5, -0.5 } };
    parts[14] = (struct vertex) { .pos = {  0.5,  0.5, -0.5 } };
    parts[15] = (struct vertex) { .pos = { -0.5,  0.5, -0.5 } };

    // Vertical edges connecting top and bottom faces
    parts[16] = (struct vertex) { .pos = { -0.5, -0.5, -0.5 } };
    parts[17] = (struct vertex) { .pos = { -0.5,  0.5, -0.5 } };
    parts[18] = (struct vertex) { .pos = { -0.5, -0.5,  0.5 } };
    parts[19] = (struct vertex) { .pos = { -0.5,  0.5,  0.5 } };
    parts[20] = (struct vertex) { .pos = {  0.5, -0.5,  0.5 } };
    parts[21] = (struct vertex) { .pos = {  0.5,  0.5,  0.5 } };
    parts[22] = (struct vertex) { .pos = {  0.5, -0.5, -0.5 } };
    parts[23] = (struct vertex) { .pos = {  0.5,  0.5, -0.5 } };

    trinity  t = trinity();
    window   w = window(t, t, width, 800, height, 800);
    shader   draw_shader = shader(t, t, name, string("cube.wgsl"));
    pipeline cube = pipeline(t, t, w, w, shader, draw_shader, read, parts);
    push(w, cube);

    loop(w);
    return  0;
}