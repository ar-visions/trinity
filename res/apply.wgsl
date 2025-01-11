struct particle {
    pos:        vec2<f32>,
    velocity:   vec2<f32>,
    density:    f32,
    pressure:   f32
};

@group(0) @binding(0) var<storage, read_write> particles: array<particle>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
    let i = id.x;
    let timestep             = 0.016f;
    let velocity_threshold   = 0.01f;
    let velocity_damping     = 0.99f;
    let boundary_damping     = 0.80f;

    if (i >= arrayLength(&particles)) { return; }

    //particles[i].velocity.x  = 0.00f;
    //particles[i].velocity.y  = -0.02f;

    // Update position based on velocity
    particles[i].pos += particles[i].velocity * timestep;

    // clamp to boundary [needs a uniform]
    if (particles[i].pos.x < -1.0) {
        particles[i].pos.x = -1.0;
        //particles[i].velocity.x *= -boundary_damping; // Reflect and dampen velocity
    } else if (particles[i].pos.x > 1.0) {
        particles[i].pos.x = 1.0;
        //particles[i].velocity.x *= -boundary_damping;
    }
    if (particles[i].pos.y < -1.0) {
        particles[i].pos.y = -1.0;
        //particles[i].velocity.y *= -boundary_damping;
    } else if (particles[i].pos.y > 1.0) {
        particles[i].pos.y = 1.0;
        //particles[i].velocity.y *= -boundary_damping;
    }

    // optional: Apply drag/damping to velocity
    particles[i].velocity *= velocity_damping;

    // optional: Reset small velocities to prevent jitter
    if (length(particles[i].velocity) < velocity_threshold) {
        particles[i].velocity = vec2<f32>(0.0, 0.0);
    }

}