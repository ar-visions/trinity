
#include <import>

double sqrt(double);
static int n_parts = 3;

int main(int argc, char *argv[]) {
    A_start();

    mat4f a      = mat4f(null);
    mat4f b      = translate(mat4f(null), vec3f(1.0f, 1.0f, 1.0f));
    mat4f r      = mul(a, b);

    path    gltf_file = path("hinge.gltf");
    Model   hinge     = read(gltf_file, typeid(Model));

    vertex parts = A_alloc(typeid(vertex), n_parts, true);

    // Bottom face edges
    parts[0] = (struct vertex) { .pos = {  0.0f, -0.5f,  0.5f } };
    parts[1] = (struct vertex) { .pos = { -0.5f,  0.5f,  0.5f } };
    parts[2] = (struct vertex) { .pos = {  0.5f,  0.5f,  0.5f } };

    trinity  t = trinity();
    window   w = window(t, t, title, string("hypercube"), width, 800, height, 800);
    shader   draw_shader = shader(t, t, vert, string("cube.vert"), frag, string("cube.frag"));
    pipeline cube = pipeline(t, t, w, w, shader, draw_shader, read, parts);
    push(w, cube);

    loop(w);
    return  0;
}