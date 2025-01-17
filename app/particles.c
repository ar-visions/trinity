
#include <import>

double sqrt(double);
static int n_parts = 256*2;

int main(int argc, char *argv[]) {
    A_start();

    /// vector allocation of particles (just 256 for now)
    particle parts = A_alloc(typeid(particle), n_parts, true);
    int sp = (int)sqrt((f64)n_parts);
    for (int x = 0; x < sp; x++) for (int y = 0; y < sp; y++) {
        int index = y * sp + x;
        particle p = &parts[index];
        p->pos.x = -0.5 + (real)x / (real)(sp - 1) * 1.0;
        p->pos.y = -0.5 + (real)y / (real)(sp - 1) * 1.0;
        p->density = 1.0f;
        p->pressure = 0.0f;
    }

    trinity  t = trinity();
    window   w = window(t, t, width, 800, height, 600);

    /// load compute / render shaders
    shader   fluid_shader = shader(t, t, comp, string("fluid.comp"));
    shader   apply_shader = shader(t, t, comp, string("apply.comp"));
    shader   draw_shader  = shader(t, t, vert, string("draw.vert"), frag, string("draw.frag"));

    /// construct pipelines for each (fluid forces, apply forces, and draw particles)
    /// notice when fluid/apply is called, the VBO data should change for draw to make use of
    pipeline fluid = pipeline(t, t, w, w, shader, fluid_shader, read_write, parts); // creates the parts buffer
    pipeline apply = pipeline(t, t, w, w, shader, apply_shader, read_write, parts); // USES the parts buffer, but can WRITE!
    pipeline draw  = pipeline(t, t, w, w, shader, draw_shader,  read,       parts); // ALSO uses the parts buffer, but READS only!

    /// push pipelines in their compute order
    push(w, fluid);
    push(w, apply);
    push(w, draw);

    loop(w);
    return  0;
}