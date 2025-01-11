// Vertex Shader
@vertex
fn vs_main(@location(0) position: vec3<f32>) -> @builtin(position) vec4<f32> {
    return vec4(position, 1.0); // Pass position directly
}

// Fragment Shader
@fragment
fn fs_main() -> @location(0) vec4<f32> {
    return vec4(1.0, 0.0, 0.0, 1.0); // Solid red color
}