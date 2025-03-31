
#include <import>

double sqrt(double);
static int n_parts = 256*2;

int main(int argc, char *argv[]) {
/*  A_start();

    /// vector allocation of particles (just 256 for now)
    VertexBuffer parts = ( A_alloc(typeid(particle), n_parts, true); /// use VBO here
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
    window   w = window(trinity, t, width, 800, height, 600);

    /// load compute / render shaders
    shader   fluid_shader = shader(trinity, t, comp, string("fluid.comp"));
    shader   apply_shader = shader(trinity, t, comp, string("apply.comp"));
    shader   draw_shader  = shader(trinity, t, vert, string("draw.vert"), frag, string("draw.frag"));

    /// construct pipelines for each (fluid forces, apply forces, and draw particles)
    /// notice when fluid/apply is called, the VBO data should change for draw to make use of
    pipeline fluid = compute_shader(trinity, fluid_shader, parts); //pipeline(trinity, t, w, w, shader, fluid_shader, memory,     parts); // creates the parts buffer
    pipeline apply = compute_shader(trinity, apply_shader, parts); //pipeline(trinity, t, w, w, shader, apply_shader, memory,     parts); // USES the parts buffer, but can WRITE!
    pipeline draw  = graphic_shader(trinity, t, w, w, shader, draw_shader,  vbo,        parts); // ALSO uses the parts buffer, but READS only!

    

    
    /// push pipelines in their compute order
    push(w, fluid);
    push(w, apply);
    push(w, draw);

    loop(w);
    return  0;
    */
}