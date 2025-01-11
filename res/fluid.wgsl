struct particle {
    pos:        vec2<f32>,
    velocity:   vec2<f32>,
    density:    f32,
    pressure:   f32
};

@group(0) @binding(0) var<storage, read_write> particles: array<particle>;

fn poly6_kernel(distance: f32, smoothing_radius: f32) -> f32 {
    let h2 = smoothing_radius * smoothing_radius;
    let r2 = distance * distance;

    if (distance >= 0.0 && distance <= smoothing_radius) {
        let factor = h2 - r2;
        return (315.0 / (64.0 * 3.14159265359 * pow(smoothing_radius, 9.0))) * factor * factor * factor;
    } else {
        return 0.0;
    }
}

fn spiky_gradient_kernel(direction: vec2<f32>, smoothing_radius: f32) -> vec2<f32> {
    let distance = length(direction);
    if (distance > 0.0 && distance <= smoothing_radius) {
        let factor = (smoothing_radius - distance) * (smoothing_radius - distance);
        let normalization = -45.0 / (3.14159265359 * pow(smoothing_radius, 6.0)); // 2D normalization constant
        return normalization * factor * normalize(direction);
    } else {
        return vec2<f32>(0.0, 0.0);
    }
}

fn laplacian_viscosity_kernel(distance: f32, smoothing_radius: f32) -> f32 {
    if (distance >= 0.0 && distance <= smoothing_radius) {
        let normalization = 45.0 / (3.14159265359 * pow(smoothing_radius, 5.0)); // 2D normalization constant
        return normalization * (smoothing_radius - distance);
    } else {
        return 0.0;
    }
}

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
    let i = id.x; // Current particle index
    let timestep           = 0.016f; // Time step for simulation (16 ms for 60 FPS)
    let stiffness_constant = 200.0f; // Controls the pressure force intensity
    let rest_density       = 1.0f;   // Reference density for the fluid
    let viscosity          = 0.1f;   // Coefficient of viscosity for smoothing
    let smoothing_radius   = 0.1f;   // Radius of influence for particles
    let particle_mass      = 1.0f;   // Mass of each particle
    let gravity            = vec2<f32>(0.0f, -9.8f); // Gravity in the simulation

    if (i >= arrayLength(&particles)) { return; }

    // Initialize local variables
    var density = 0.0;
    var force = gravity * particle_mass * 0.0f; // Start with gravitational force

    // Loop through all particles
    for (var j = 0u; j < arrayLength(&particles); j++) {
        if (i == j) { continue; }

        let r = particles[j].pos - particles[i].pos;
        let dist = length(r);

        if (dist < smoothing_radius) {
            // Accumulate density using kernel function
            density += particle_mass * poly6_kernel(dist, smoothing_radius);

            // Compute pressure force using gradient kernel
            let pressure_term = (particles[i].pressure + particles[j].pressure) / (2.0 * particles[j].density);
            force -= pressure_term * 0.000000001f * spiky_gradient_kernel(r, smoothing_radius);

            // Add viscosity force
            force += viscosity * (particles[j].velocity - particles[i].velocity) * 
                laplacian_viscosity_kernel(dist, smoothing_radius);
        }
    }

    // Update density and pressure for the current particle
    particles[i].density = density;
    particles[i].pressure = stiffness_constant * max(0.0, density - rest_density);

    // Apply accumulated forces to velocity
    particles[i].velocity += force * timestep;
}