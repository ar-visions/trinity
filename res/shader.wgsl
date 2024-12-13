/*@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4<f32> {
    let x = f32(i32(in_vertex_index) - 1);
    let y = f32(i32(in_vertex_index & 1u) * 2 - 1);
    return vec4<f32>(x, y, 0.0, 1.0);
}*/

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4<f32> {
    var pos = array<vec2<f32>, 3>(
        vec2<f32>( 0.0,  0.5),  // top
        vec2<f32>(-0.5, -0.5),  // bottom left
        vec2<f32>( 0.5, -0.5)   // bottom right
    );
    return vec4<f32>(pos[in_vertex_index], 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
    return vec4<f32>(1.0, 1.0, 1.0, 1.0);
}
/*
const char* shaderSource = R"(

struct Uniforms {
    resolution: vec2f,
    time: f32,
    padding: f32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

// Perlin noise implementation
fn hash(p: vec3f) -> f32 {
    var p3 = fract(vec3f(p.x, p.y, p.z) * 0.13);
    p3 += dot(p3, p3.yzx + 3.333);
    return fract((p3.x + p3.y) * p3.z);
}

fn noise(p: vec3f) -> f32 {
    let i = floor(p);
    let f = fract(p);
    
    let a = hash(i);
    let b = hash(i + vec3f(1.0, 0.0, 0.0));
    let c = hash(i + vec3f(0.0, 1.0, 0.0));
    let d = hash(i + vec3f(1.0, 1.0, 0.0));
    let e = hash(i + vec3f(0.0, 0.0, 1.0));
    let f1 = hash(i + vec3f(1.0, 0.0, 1.0));
    let g = hash(i + vec3f(0.0, 1.0, 1.0));
    let h = hash(i + vec3f(1.0, 1.0, 1.0));
    
    let u = f * f * (3.0 - 2.0 * f);
    
    return mix(
        mix(mix(a, b, u.x), mix(c, d, u.x), u.y),
        mix(mix(e, f1, u.x), mix(g, h, u.x), u.y),
        u.z
    );
}

fn intersectCylinder(ro: vec3f, rd: vec3f, h: f32, r: f32) -> vec2f {
    let a = rd.x * rd.x + rd.z * rd.z;
    let b = 2.0 * (ro.x * rd.x + ro.z * rd.z);
    let c = ro.x * ro.x + ro.z * ro.z - r * r;
    
    var t = vec2f(-1.0);
    let discriminant = b * b - 4.0 * a * c;
    
    if (discriminant >= 0.0) {
        let q = sqrt(discriminant);
        t = vec2f((-b - q) / (2.0 * a), (-b + q) / (2.0 * a));
        
        let y1 = ro.y + t.x * rd.y;
        let y2 = ro.y + t.y * rd.y;
        
        if (y1 > h || y1 < -h) { t.x = -1.0; }
        if (y2 > h || y2 < -h) { t.y = -1.0; }
    }
    
    return t;
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn vs_main(@builtin(vertex_index) VertexIndex : u32) -> VertexOutput {
    var pos = array<vec2f, 4>(
        vec2f(-1.0, -1.0),
        vec2f( 1.0, -1.0),
        vec2f(-1.0,  1.0),
        vec2f( 1.0,  1.0)
    );
    
    var output: VertexOutput;
    output.position = vec4f(pos[VertexIndex], 0.0, 1.0);
    output.uv = pos[VertexIndex];
    return output;
}

@fragment
fn fs_main(@location(0) uv: vec2f) -> @location(0) vec4f {
    let resolution = uniforms.resolution;
    let fragCoord = (uv + 1.0) * 0.5 * resolution;
    let normalizedCoord = (fragCoord * 2.0 - resolution) / min(resolution.x, resolution.y);
    
    let ro = vec3f(0.0, 0.0, 4.0);
    let rd = normalize(vec3f(normalizedCoord, -1.0));
    
    let cylinderHeight = 1.0;
    let cylinderRadius = 1.0;
    
    let t = intersectCylinder(ro, rd, cylinderHeight, cylinderRadius);
    
    if (t.x < 0.0 && t.y < 0.0) {
        return vec4f(0.0, 0.0, 0.0, 1.0);
    }
    
    let stepSize = 0.05;
    let numSteps = 64;
    var accumulation = 0.0;
    
    let start = max(t.x, 0.0);
    let end = t.y;
    
    for (var i = 0; i < numSteps; i++) {
        let currentT = mix(start, end, f32(i) / f32(numSteps));
        let pos = ro + rd * currentT;
        
        let noiseValue = noise(pos * 3.0 + vec3f(0.0, uniforms.time * 0.2, 0.0));
        accumulation += noiseValue * stepSize;
    }
    
    let color = vec3f(0.2, 0.4, 0.8) * accumulation;
    return vec4f(color, 1.0);
}
*/