// Initialize WebGPU
async function initWebGPU() {
    if (!navigator.gpu) {
        throw new Error('WebGPU not supported');
    }

    const adapter = await navigator.gpu.requestAdapter();
    const device = await adapter.requestDevice();

    const canvas = document.createElement('canvas');
    canvas.width = 800;
    canvas.height = 600;
    document.body.appendChild(canvas);

    const context = canvas.getContext('webgpu');
    const format = navigator.gpu.getPreferredCanvasFormat();

    context.configure({
        device,
        format,
        alphaMode: 'premultiplied',
    });

    return { device, context, format, canvas };
}

// Shader code
const shaderCode = `
struct Uniforms {
    resolution: vec2f,
    time: f32,
}

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

// Ray-cylinder intersection
fn intersectCylinder(ro: vec3f, rd: vec3f, h: f32, r: f32) -> vec2f {
    let a = rd.x * rd.x + rd.z * rd.z;
    let b = 2.0 * (ro.x * rd.x + ro.z * rd.z);
    let c = ro.x * ro.x + ro.z * ro.z - r * r;
    
    var t = vec2f(-1.0);
    let discriminant = b * b - 4.0 * a * c;
    
    if (discriminant >= 0.0) {
        let q = sqrt(discriminant);
        t = vec2f((-b - q) / (2.0 * a), (-b + q) / (2.0 * a));
        
        // Check height bounds
        let y1 = ro.y + t.x * rd.y;
        let y2 = ro.y + t.y * rd.y;
        
        if (y1 > h || y1 < -h) { t.x = -1.0; }
        if (y2 > h || y2 < -h) { t.y = -1.0; }
    }
    
    return t;
}

@fragment
fn main(@location(0) fragCoord: vec2f) -> @location(0) vec4f {
    let resolution = uniforms.resolution;
    let uv = (fragCoord * 2.0 - resolution) / min(resolution.x, resolution.y);
    
    // Ray setup
    let ro = vec3f(0.0, 0.0, 4.0); // Ray origin
    let rd = normalize(vec3f(uv, -1.0)); // Ray direction
    
    // Cylinder parameters
    let cylinderHeight = 1.0;
    let cylinderRadius = 1.0;
    
    // Get cylinder intersection points
    let t = intersectCylinder(ro, rd, cylinderHeight, cylinderRadius);
    
    if (t.x < 0.0 && t.y < 0.0) {
        return vec4f(0.0, 0.0, 0.0, 1.0);
    }
    
    // Ray marching parameters
    let stepSize = 0.05;
    let numSteps = 64;
    var accumulation = 0.0;
    
    // Start and end points for ray marching
    let start = max(t.x, 0.0);
    let end = t.y;
    
    // Ray march through the volume
    for (var i = 0; i < numSteps; i++) {
        let currentT = mix(start, end, f32(i) / f32(numSteps));
        let pos = ro + rd * currentT;
        
        // Sample noise at current position
        let noiseValue = noise(pos * 3.0 + vec3f(0.0, uniforms.time * 0.2, 0.0));
        accumulation += noiseValue * stepSize;
    }
    
    // Visualize the result
    let color = vec3f(0.2, 0.4, 0.8) * accumulation;
    return vec4f(color, 1.0);
}
`;

async function main() {
    const { device, context, format, canvas } = await initWebGPU();

    // Create uniform buffer
    const uniformBuffer = device.createBuffer({
        size: 16, // vec2 resolution + float time + padding
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    // Create pipeline
    const pipeline = device.createRenderPipeline({
        layout: 'auto',
        vertex: {
            module: device.createShaderModule({
                code: `
                    @vertex
                    fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4f {
                        var pos = array<vec2f, 4>(
                            vec2f(-1.0, -1.0),
                            vec2f( 1.0, -1.0),
                            vec2f(-1.0,  1.0),
                            vec2f( 1.0,  1.0)
                        );
                        return vec4f(pos[VertexIndex], 0.0, 1.0);
                    }
                `
            }),
            entryPoint: 'main',
        },
        fragment: {
            module: device.createShaderModule({
                code: shaderCode,
            }),
            entryPoint: 'main',
            targets: [{
                format: format,
            }],
        },
        primitive: {
            topology: 'triangle-strip',
            stripIndexFormat: undefined,
        },
    });

    // Create bind group
    const bindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [{
            binding: 0,
            resource: {
                buffer: uniformBuffer,
            },
        }],
    });

    // Animation frame
    function frame(time) {
        const uniforms = new Float32Array([
            canvas.width, canvas.height,
            time * 0.001,
            0.0, // padding
        ]);
        device.queue.writeBuffer(uniformBuffer, 0, uniforms);

        const commandEncoder = device.createCommandEncoder();
        const passEncoder = commandEncoder.beginRenderPass({
            colorAttachments: [{
                view: context.getCurrentTexture().createView(),
                clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
                loadOp: 'clear',
                storeOp: 'store',
            }],
        });

        passEncoder.setPipeline(pipeline);
        passEncoder.setBindGroup(0, bindGroup);
        passEncoder.draw(4, 1, 0, 0);
        passEncoder.end();

        device.queue.submit([commandEncoder.finish()]);
        requestAnimationFrame(frame);
    }

    requestAnimationFrame(frame);
}

main();