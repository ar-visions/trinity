struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}

struct Uniforms {
    view_proj: mat4x4<f32>,
    time: f32,
    _padding: vec3<f32>,
}

@group(0) @binding(0)
var<uniform> uniforms: Uniforms;

@vertex
fn vs_main(@location(0) position: vec2<f32>) -> VertexOutput {
    var out: VertexOutput;
    out.clip_position = vec4<f32>(position, 0.0, 1.0);
    out.uv = position;
    return out;
}


// Permutation table
fn permute4(x: vec4<f32>) -> vec4<f32> {
    return ((x * 34.0 + 1.0) * x) % vec4<f32>(289.0);
}

fn taylorInvSqrt4(r: vec4<f32>) -> vec4<f32> {
    return 1.79284291400159 - 0.85373472095314 * r;
}

fn fade(t: vec3<f32>) -> vec3<f32> {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

fn perlin3D(P: vec3<f32>) -> f32 {
    var Pi0 : vec3<f32> = floor(P); // Integer part
    var Pi1 : vec3<f32> = Pi0 + vec3<f32>(1.0); // Integer part + 1
    Pi0 = Pi0 % vec3<f32>(289.0);
    Pi1 = Pi1 % vec3<f32>(289.0);
    let Pf0 = fract(P); // Fractional part
    let Pf1 = Pf0 - vec3<f32>(1.0); // Fractional part - 1.0
    
    let ix = vec4<f32>(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
    let iy = vec4<f32>(Pi0.yy, Pi1.yy);
    let iz0 = Pi0.zzzz;
    let iz1 = Pi1.zzzz;

    let ixy = permute4(permute4(ix) + iy);
    let ixy0 = permute4(ixy + iz0);
    let ixy1 = permute4(ixy + iz1);

    var gx0: vec4<f32> = ixy0 / 7.0;
    var gy0: vec4<f32> = fract(floor(gx0) / 7.0) - 0.5;
    gx0 = fract(gx0);
    var gz0: vec4<f32> = vec4<f32>(0.5) - abs(gx0) - abs(gy0);
    var sz0: vec4<f32> = step(gz0, vec4<f32>(0.0));
    gx0 = gx0 + sz0 * (step(vec4<f32>(0.0), gx0) - 0.5);
    gy0 = gy0 + sz0 * (step(vec4<f32>(0.0), gy0) - 0.5);

    var gx1: vec4<f32> = ixy1 / 7.0;
    var gy1: vec4<f32> = fract(floor(gx1) / 7.0) - 0.5;
    gx1 = fract(gx1);
    var gz1: vec4<f32> = vec4<f32>(0.5) - abs(gx1) - abs(gy1);
    var sz1: vec4<f32> = step(gz1, vec4<f32>(0.0));
    gx1 = gx1 + sz1 * (step(vec4<f32>(0.0), gx1) - 0.5);
    gy1 = gy1 + sz1 * (step(vec4<f32>(0.0), gy1) - 0.5);

    var g000: vec3<f32> = vec3<f32>(gx0.x, gy0.x, gz0.x);
    var g100: vec3<f32> = vec3<f32>(gx0.y, gy0.y, gz0.y);
    var g010: vec3<f32> = vec3<f32>(gx0.z, gy0.z, gz0.z);
    var g110: vec3<f32> = vec3<f32>(gx0.w, gy0.w, gz0.w);
    var g001: vec3<f32> = vec3<f32>(gx1.x, gy1.x, gz1.x);
    var g101: vec3<f32> = vec3<f32>(gx1.y, gy1.y, gz1.y);
    var g011: vec3<f32> = vec3<f32>(gx1.z, gy1.z, gz1.z);
    var g111: vec3<f32> = vec3<f32>(gx1.w, gy1.w, gz1.w);

    let norm0 = taylorInvSqrt4(vec4<f32>(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
    g000 = g000 * norm0.x;
    g010 = g010 * norm0.y;
    g100 = g100 * norm0.z;
    g110 = g110 * norm0.w;
    let norm1 = taylorInvSqrt4(vec4<f32>(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
    g001 = g001 * norm1.x;
    g011 = g011 * norm1.y;
    g101 = g101 * norm1.z;
    g111 = g111 * norm1.w;

    let n000 = dot(g000, Pf0);
    let n100 = dot(g100, vec3<f32>(Pf1.x, Pf0.yz));
    let n010 = dot(g010, vec3<f32>(Pf0.x, Pf1.y, Pf0.z));
    let n110 = dot(g110, vec3<f32>(Pf1.xy, Pf0.z));
    let n001 = dot(g001, vec3<f32>(Pf0.xy, Pf1.z));
    let n101 = dot(g101, vec3<f32>(Pf1.x, Pf0.y, Pf1.z));
    let n011 = dot(g011, vec3<f32>(Pf0.x, Pf1.yz));
    let n111 = dot(g111, Pf1);

    let fade_xyz = fade(Pf0);
    let n_z = mix(vec4<f32>(n000, n100, n010, n110), vec4<f32>(n001, n101, n011, n111), fade_xyz.z);
    let n_yz = mix(n_z.xy, n_z.zw, fade_xyz.y);
    let n_xyz = mix(n_yz.x, n_yz.y, fade_xyz.x);
    
    return n_xyz * 0.5 + 0.5;
}

// Fractal Brownian Motion with the new Perlin noise
fn fbm(pos: vec3<f32>, y_trans: f32) -> f32 {
    var value = 0.0;
    var amplitude = 0.5;
    var frequency = 1.0;
    
    for(var i = 0; i < 6; i++) {
        value += amplitude * perlin3D(pos * frequency + vec3<f32>(0.0, y_trans * 4.0, 0.0));
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    
    return value;
}


fn sdf_cylinder(p: vec3<f32>, h: f32, r: f32) -> f32 {
    // Make radius larger based on height
    var varying_radius = r; //r * (1.0 + p.y * 0.5);  // Adjust the 0.5 multiplier to control taper
    
    let d = abs(vec2<f32>(length(p.xz), p.y)) - vec2<f32>(varying_radius, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, vec2<f32>(0.0)));
}

fn rotateY(angle: f32) -> mat3x3<f32> {
    let c = cos(angle);
    let s = sin(angle);
    return mat3x3<f32>(
        vec3<f32>(c, 0.0, -s),
        vec3<f32>(0.0, 1.0, 0.0),
        vec3<f32>(s, 0.0, c)
    );
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let aspect            = 1920.0 / 1080.0;
    var ray_dir           = normalize(vec3<f32>(in.uv.x * aspect, in.uv.y, -1.0));
    let ray_origin        = vec3<f32>(0.0, 0.0, 5.0);
    let noise_rotation    = rotateY(uniforms.time * 0.88);
    let y_translate       = uniforms.time * -0.88 * 0.88;
    var accumulated_color = vec3<f32>(0.0);
    var accumulated_alpha = 0.0;
    var entry_t           = 0.0;
    var exit_t            = 0.0;
    var t                 = 0.0;
    var inside            = false;
    
    for(var i = 0; i < 44; i++) {
        let pos = ray_origin + ray_dir * t;
        let cylinder_pos = pos - vec3<f32>(0.0, 0.0, 3.0);
        let d = sdf_cylinder(cylinder_pos, 1.0, 0.5);
        
        if(!inside && d < 0.001) {
            entry_t = t;
            inside = true;
        } else if(inside && d > 0.001) {
            exit_t = t;
            break;
        }
        
        t += max(abs(d), 0.01);
    }
    
    if(inside) {
        let num_steps = 88;
        let dist = exit_t - entry_t;
        let step_size = dist / f32(num_steps);
        t = entry_t;

        for(var i = 0; i < num_steps; i++) {
            let pos = ray_origin + ray_dir * t;
            let sample_pos = pos - vec3<f32>(0.0, 0.0, 3.0);
            let height_factor = 1.0 - (sample_pos.y + 1.0) * 0.5;  // This maps from [-1,1] to [0,1]

                // Create twist based on height
            let twist_amount = sample_pos.y * height_factor * 6.0;  // Adjust multiplier to control twist intensity
            let twist = mat3x3<f32>(
                cos(twist_amount), 0.0, -sin(twist_amount),
                0.0, 1.0, 0.0,
                sin(twist_amount), 0.0, cos(twist_amount)
            );

            // Apply both twist and rotation
            let twisted_pos      = twist * sample_pos;
            let rotated_pos      = noise_rotation * twisted_pos;
            let distance_to_wall = abs(sdf_cylinder(sample_pos, 1.0, 0.5));
            let dist_to_center   = length(sample_pos.xz);  // Distance from central axis
            let center_f         = 1.0 - (dist_to_center / 0.5);  // 1.0 at center, 0.0 at wall

            // In your fragment shader, where you sample the noise:
            let raw_density = fbm(rotated_pos * 2.0, y_translate);

            // Remap values with threshold
            let threshold    = 0.44;  // Adjust this value to control where black starts
            let contrast     = center_f * 8.0;   // Adjust this to control how sharp the transition is
            var density      = max(0.0, (raw_density - threshold) * contrast);
            let sample_alpha = density;
            let sample_color = vec3<f32>(density);
            
            accumulated_color += distance_to_wall * (1.0 - accumulated_alpha) * sample_color * sample_alpha;
            accumulated_alpha += distance_to_wall * (1.0 - accumulated_alpha) * sample_alpha;

            t += step_size;
        }
    }
    
    return vec4<f32>(accumulated_color, accumulated_alpha);
}