// example how to set up WebGPU rendering on Windows in C
// uses Dawn implementation of WebGPU: https://dawn.googlesource.com/dawn/
// download pre-built Dawn webgpu.h/dll/lib files from https://github.com/mmozeiko/build-dawn/releases/latest

#include "webgpu.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>

// replace this with your favorite Assert() implementation
#include <intrin.h>
#define Assert(cond) do { if (!(cond)) __debugbreak(); } while (0)

#pragma comment (lib, "gdi32")
#pragma comment (lib, "user32")
#pragma comment (lib, "webgpu_dawn")

#define WEBGPU_STR(str) (WGPUStringView) { .data = str, .length = sizeof(str) - 1 }

static void FatalError(const char* message)
{
    MessageBoxA(NULL, message, "Error", MB_ICONEXCLAMATION);
    ExitProcess(0);
}

static void OnDeviceError(WGPUErrorType type, const char* message, void* userdata)
{
    FatalError(message);
}

static void OnAdapterRequestEnded(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata)
{
    if (status != WGPURequestAdapterStatus_Success)
    {
        // cannot find adapter?
        FatalError(message);
    }
    else
    {
        // use first adapter provided
        WGPUAdapter* result = userdata;
        if (*result == NULL)
        {
            *result = adapter;
        }
    }
}

static LRESULT CALLBACK WindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(wnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE previnstance, LPSTR cmdline, int cmdshow)
{
    // register window class to have custom WindowProc callback
    WNDCLASSEXW wc =
    {
        .cbSize = sizeof(wc),
        .lpfnWndProc = &WindowProc,
        .hInstance = hinstance,
        .hIcon = LoadIcon(NULL, IDI_APPLICATION),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = L"webgpu_window_class",
    };
    ATOM atom = RegisterClassExW(&wc);
    Assert(atom && "Failed to register window class");

    // window properties - width, height and style
    int width = CW_USEDEFAULT;
    int height = CW_USEDEFAULT;
    DWORD exstyle = WS_EX_APPWINDOW;
    DWORD style = WS_OVERLAPPEDWINDOW;

    // uncomment in case you want fixed size window
    //style &= ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    //RECT rect = { 0, 0, 1280, 720 };
    //AdjustWindowRectEx(&rect, style, FALSE, exstyle);
    //width = rect.right - rect.left;
    //height = rect.bottom - rect.top;

    // create window
    HWND window = CreateWindowExW(
        exstyle, wc.lpszClassName, L"WebGPU Window", style,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, wc.hInstance, NULL);
    Assert(window && "Failed to create window");

    // ============================================================================================

    struct Vertex
    {
        float position[2];
        float uv[2];
        float color[3];
    };
    const uint32_t kVertexStride = sizeof(struct Vertex);

    static const struct Vertex kVertexData[] =
    {
        { { -0.00f, +0.75f }, { 25.0f, 50.0f }, { 1, 0, 0 } },
        { { +0.75f, -0.50f }, {  0.0f,  0.0f }, { 0, 1, 0 } },
        { { -0.75f, -0.50f }, { 50.0f,  0.0f }, { 0, 0, 1 } },
    };

    const uint32_t kTextureWidth = 2;
    const uint32_t kTextureHeight = 2;

    // ============================================================================================

    const WGPUTextureFormat kSwapChainFormat = WGPUTextureFormat_BGRA8Unorm;

    // optionally use WGPUInstanceDescriptor::nextInChain for WGPUDawnTogglesDescriptor
    // with various toggles enabled or disabled: https://dawn.googlesource.com/dawn/+/refs/heads/main/src/dawn/native/Toggles.cpp
    WGPUInstance instance = wgpuCreateInstance(NULL);
    Assert(instance && "Failed to create WebGPU instance");

    WGPUSurface surface;
    {
        WGPUSurfaceDescriptor desc =
        {
            .nextInChain = &((WGPUSurfaceSourceWindowsHWND)
            {
                .chain.sType = WGPUSType_SurfaceSourceWindowsHWND,
                .hinstance = wc.hInstance,
                .hwnd = window,
            }).chain,
        };
        surface = wgpuInstanceCreateSurface(instance, &desc);
        Assert(surface && "Failed to create WebGPU surface");
    }

    WGPUAdapter adapter = NULL;
    {
        WGPURequestAdapterOptions options =
        {
            .compatibleSurface = surface,
            // .powerPreference = WGPUPowerPreference_HighPerformance,
        };
        wgpuInstanceRequestAdapter(instance, &options, &OnAdapterRequestEnded, &adapter);
        Assert(adapter && "Failed to get WebGPU adapter");

        // can query extra details on what adapter supports:
        // wgpuAdapterEnumerateFeatures
        // wgpuAdapterGetLimits
        // wgpuAdapterGetProperties
        // wgpuAdapterHasFeature

        {
            WGPUAdapterInfo info = { 0 };
            wgpuAdapterGetInfo(adapter, &info);

            const char* adapter_types[] =
            {
                [WGPUAdapterType_DiscreteGPU] = "Discrete GPU",
                [WGPUAdapterType_IntegratedGPU] = "Integrated GPU",
                [WGPUAdapterType_CPU] = "CPU",
                [WGPUAdapterType_Unknown] = "unknown",
            };

            char temp[1024];
            snprintf(temp, sizeof(temp),
                "Device        = %.*s\n"
                "Description   = %.*s\n"
                "Vendor        = %.*s\n"
                "Architecture  = %.*s\n"
                "Adapter Type  = %s\n",
                (int)info.device.length, info.device.data,
                (int)info.description.length, info.description.data,
                (int)info.vendor.length, info.vendor.data,
                (int)info.architecture.length, info.architecture.data,
                adapter_types[info.adapterType]);

            OutputDebugStringA(temp);
        }
    }

    WGPUDevice device = NULL;
    {
        // if you want to be sure device will support things you'll use, you can specify requirements here:

        //WGPUSupportedLimits supported = { 0 };
        //wgpuAdapterGetLimits(adapter, &supported);

        //supported.limits.maxTextureDimension2D = kTextureWidth;
        //supported.limits.maxBindGroups = 1;
        //supported.limits.maxBindingsPerBindGroup = 3; // uniform buffer for vertex shader, and texture + sampler for fragment
        //supported.limits.maxSampledTexturesPerShaderStage = 1;
        //supported.limits.maxSamplersPerShaderStage = 1;
        //supported.limits.maxUniformBuffersPerShaderStage = 1;
        //supported.limits.maxUniformBufferBindingSize = 4 * 4 * sizeof(float); // 4x4 matrix
        //supported.limits.maxVertexBuffers = 1;
        //supported.limits.maxBufferSize = sizeof(kVertexData);
        //supported.limits.maxVertexAttributes = 3; // pos, texcoord, color
        //supported.limits.maxVertexBufferArrayStride = kVertexStride;
        //supported.limits.maxColorAttachments = 1;

        WGPUDeviceDescriptor desc =
        {
            // notify on errors
            .uncapturedErrorCallbackInfo.callback = &OnDeviceError,

            // extra features: https://dawn.googlesource.com/dawn/+/refs/heads/main/src/dawn/native/Features.cpp
            //.requiredFeaturesCount = n
            //.requiredFeatures = (WGPUFeatureName[]) { ... }
            //.requiredLimits = &(WGPURequiredLimits) { .limits = supported.limits },
        };

        device = wgpuAdapterCreateDevice(adapter, &desc);
        Assert(device && "Failed to create WebGPU device");
    }

    // default device queue
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    // ============================================================================================

    WGPUBuffer vbuffer;
    {
        WGPUBufferDescriptor desc =
        {
            .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
            .size = sizeof(kVertexData),
        };
        vbuffer = wgpuDeviceCreateBuffer(device, &desc);
        wgpuQueueWriteBuffer(queue, vbuffer, 0, kVertexData, sizeof(kVertexData));
    }

    // uniform buffer for one 4x4 float matrix
    WGPUBuffer ubuffer;
    {
        WGPUBufferDescriptor desc =
        {
            .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
            .size = 4 * 4 * sizeof(float), // 4x4 matrix
        };
        ubuffer = wgpuDeviceCreateBuffer(device, &desc);
    }

    WGPUTextureView texture_view;
    {
        // checkerboard texture, with 50% transparency on black colors
        unsigned int pixels[] =
        {
            0x80000000, 0xffffffff,
            0xffffffff, 0x80000000,
        };

        WGPUTextureDescriptor desc =
        {
            .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
            .dimension = WGPUTextureDimension_2D,
            .size = { kTextureWidth, kTextureHeight, 1 },
            .format = WGPUTextureFormat_RGBA8Unorm,
            .mipLevelCount = 1,
            .sampleCount = 1,
        };
        WGPUTexture texture = wgpuDeviceCreateTexture(device, &desc);

        // upload pixels
        WGPUImageCopyTexture dst =
        {
            .texture = texture,
            .mipLevel = 0,
        };
        WGPUTextureDataLayout src =
        {
            .offset = 0,
            .bytesPerRow = kTextureWidth * 4, // 4 bytes per pixel
            .rowsPerImage = kTextureHeight,
        };
        wgpuQueueWriteTexture(queue, &dst, pixels, sizeof(pixels), &src, &desc.size);

        WGPUTextureViewDescriptor view_desc =
        {
            .format = desc.format,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All,
        };
        texture_view = wgpuTextureCreateView(texture, &view_desc);

        wgpuTextureRelease(texture);
    }

    // compile shader
    WGPUShaderModule shaders;
    {
        static const char wgsl[] =
            "struct VertexIn {                                                   \n"
            "    @location(0) aPos : vec2f,                                      \n"
            "    @location(1) aTex : vec2f,                                      \n"
            "    @location(2) aCol : vec3f                                       \n"
            "}                                                                   \n"
            "                                                                    \n"
            "struct VertexOut {                                                  \n"
            "    @builtin(position) vPos : vec4f,                                \n"
            "    @location(0)       vTex : vec2f,                                \n"
            "    @location(1)       vCol : vec3f                                 \n"
            "}                                                                   \n"
            "                                                                    \n"
            "@group(0) @binding(0) var<uniform> uTransform : mat4x4f;            \n"
            "@group(0) @binding(1) var myTexture : texture_2d<f32>;              \n"
            "@group(0) @binding(2) var mySampler : sampler;                      \n"
            "                                                                    \n"
            "@vertex                                                             \n"
            "fn vs(in : VertexIn) -> VertexOut {                                 \n"
            "    var out : VertexOut;                                            \n"
            "    out.vPos = uTransform * vec4f(in.aPos, 0.0, 1.0);               \n"
            "    out.vTex = in.aTex;                                             \n"
            "    out.vCol = in.aCol;                                             \n"
            "    return out;                                                     \n"
            "}                                                                   \n"
            "                                                                    \n"
            "@fragment                                                           \n"
            "fn fs(in : VertexOut) -> @location(0) vec4f {                       \n"
            "    var tex : vec4f = textureSample(myTexture, mySampler, in.vTex); \n"
            "    return vec4f(in.vCol, 1.0) * tex;                               \n"
            "}                                                                   \n"
            ;

        WGPUShaderModuleDescriptor desc =
        {
            .nextInChain = &((WGPUShaderSourceWGSL)
            {
                .chain.sType = WGPUSType_ShaderSourceWGSL,
                .code = WEBGPU_STR(wgsl),
            }).chain,
        };
        shaders = wgpuDeviceCreateShaderModule(device, &desc);
        // to get compiler error messages explicitly use wgpuShaderModuleGetCompilationInfo
        // otherwise they will be reported with device error callback
    }

    WGPUSampler sampler;
    {
        WGPUSamplerDescriptor desc =
        {
            .addressModeU = WGPUAddressMode_Repeat,
            .addressModeV = WGPUAddressMode_Repeat,
            .addressModeW = WGPUAddressMode_Repeat,
            .magFilter = WGPUFilterMode_Nearest,
            .minFilter = WGPUFilterMode_Nearest,
            .mipmapFilter = WGPUMipmapFilterMode_Nearest,
            .lodMinClamp = 0.f,
            .lodMaxClamp = 1.f,
            .compare = WGPUCompareFunction_Undefined,
            .maxAnisotropy = 1,
        };
        sampler = wgpuDeviceCreateSampler(device, &desc);
    }

    WGPUBindGroup bind_group;
    WGPURenderPipeline pipeline;
    {
        WGPUBindGroupLayoutDescriptor bind_group_layout_desc =
        {
            .entryCount = 3,
            .entries = (WGPUBindGroupLayoutEntry[])
            {
                // uniform buffer for vertex shader
                {
                    .binding = 0,
                    .visibility = WGPUShaderStage_Vertex,
                    .buffer.type = WGPUBufferBindingType_Uniform,
                },

                // texture for fragment shader
                {
                    .binding = 1,
                    .visibility = WGPUShaderStage_Fragment,
                    .texture.sampleType = WGPUTextureSampleType_Float,
                    .texture.viewDimension = WGPUTextureViewDimension_2D,
                    .texture.multisampled = 0,
                },

                // sampler for fragment shader
                {
                    .binding = 2,
                    .visibility = WGPUShaderStage_Fragment,
                    .sampler.type = WGPUSamplerBindingType_Filtering,
                },
            },
        };
        WGPUBindGroupLayout bind_group_layout = wgpuDeviceCreateBindGroupLayout(device, &bind_group_layout_desc);

        WGPUPipelineLayoutDescriptor pipeline_layout_desc =
        {
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = (WGPUBindGroupLayout[]) { bind_group_layout },
        };
        WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(device, &pipeline_layout_desc);

        WGPURenderPipelineDescriptor pipeline_desc =
        {
            .layout = pipeline_layout,

            // draw triangle list, no index buffer, no culling
            .primitive =
            {
                .topology = WGPUPrimitiveTopology_TriangleList,
                // .stripIndexFormat = WGPUIndexFormat_Uint16,
                .frontFace = WGPUFrontFace_CCW,
                .cullMode = WGPUCullMode_None,
            },

            // vertex shader
            .vertex =
            {
                .module = shaders,
                .entryPoint = WEBGPU_STR("vs"),
                .bufferCount = 1,
                .buffers = (WGPUVertexBufferLayout[])
                {
                    // one vertex buffer as input
                    {
                        .arrayStride = kVertexStride,
                        .stepMode = WGPUVertexStepMode_Vertex,
                        .attributeCount = 3,
                        .attributes = (WGPUVertexAttribute[])
                        {
                            { WGPUVertexFormat_Float32x2, offsetof(struct Vertex, position), 0 },
                            { WGPUVertexFormat_Float32x2, offsetof(struct Vertex, uv),       1 },
                            { WGPUVertexFormat_Float32x3, offsetof(struct Vertex, color),    2 },
                        },
                    },
                },
            },

            // fragment shader
            .fragment = &(WGPUFragmentState)
            {
                .module = shaders,
                .entryPoint = WEBGPU_STR("fs"),
                .targetCount = 1,
                .targets = (WGPUColorTargetState[])
                {
                    // writing to one output, with alpha-blending enabled
                    {
                        .format = kSwapChainFormat,
                        .blend = &(WGPUBlendState)
                        {
                            .color.operation = WGPUBlendOperation_Add,
                            .color.srcFactor = WGPUBlendFactor_SrcAlpha,
                            .color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                            .alpha.operation = WGPUBlendOperation_Add,
                            .alpha.srcFactor = WGPUBlendFactor_SrcAlpha,
                            .alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                        },
                        .writeMask = WGPUColorWriteMask_All,
                    },
                },
            },

            // if depth/stencil buffer usage/testing is needed
            //.depthStencil = &(WGPUDepthStencilState) { ... },

            // no multisampling
            .multisample =
            {
                .count = 1,
                .mask = 0xffffffff,
                .alphaToCoverageEnabled = 0,
            },
        };

        pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeline_desc);
        wgpuPipelineLayoutRelease(pipeline_layout);

        WGPUBindGroupDescriptor bind_group_desc =
        {
            .layout = bind_group_layout,
            .entryCount = 3,
            .entries = (WGPUBindGroupEntry[])
            {
                // uniform buffer for vertex shader
                { .binding = 0, .buffer = ubuffer, .offset = 0, .size = 4 * 4 * sizeof(float) },

                // texure for fragment shader
                { .binding = 1, .textureView = texture_view },

                // sampler for fragment shader
                { .binding = 2, .sampler = sampler },
            },
        };
        bind_group = wgpuDeviceCreateBindGroup(device, &bind_group_desc);
        wgpuBindGroupLayoutRelease(bind_group_layout);
    }

    // release resources that are now owned by pipeline and will not be used in this code later
    wgpuSamplerRelease(sampler);
    wgpuTextureViewRelease(texture_view);

    // ============================================================================================

    // show the window
    ShowWindow(window, SW_SHOWDEFAULT);

    LARGE_INTEGER freq, c1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c1);

    float angle = 0;
    int current_width = 0;
    int current_height = 0;

    // ============================================================================================

    bool swap_chain = false;

    for (;;)
    {
        // process all incoming Windows messages
        MSG msg;
        if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }

        // get current size for window client area
        RECT rect;
        GetClientRect(window, &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;

        // process all internal async events or error callbacks
        // this is native-code specific functionality, because in browser's WebGPU everything works with JS event loop
        wgpuInstanceProcessEvents(instance);

        // resize swap chain if needed
        if (!swap_chain || width != current_width || height != current_height)
        {
            if (swap_chain)
            {
                // release old swap chain
                wgpuSurfaceUnconfigure(surface);
                swap_chain = false;
            }

            // resize to new size for non-zero window size
            if (width != 0 && height != 0)
            {
                WGPUSurfaceConfiguration config =
                {
                    .device = device,
                    .format = kSwapChainFormat,
                    .usage = WGPUTextureUsage_RenderAttachment,
                    .width = width,
                    .height = height,
                    .presentMode = WGPUPresentMode_Fifo, // WGPUPresentMode_Mailbox // WGPUPresentMode_Immediate
                };
                wgpuSurfaceConfigure(surface, &config);
                swap_chain = true;
            }

            current_width = width;
            current_height = height;
        }

        LARGE_INTEGER c2;
        QueryPerformanceCounter(&c2);
        float delta = (float)((double)(c2.QuadPart - c1.QuadPart) / freq.QuadPart);
        c1 = c2;

        // render only if window size is non-zero, which means swap chain is created
        if (swap_chain)
        {
            // update 4x4 rotation matrix in uniform
            {
                angle += delta * 2.0f * (float)M_PI / 20.0f; // full rotation in 20 seconds
                angle = fmodf(angle, 2.0f * (float)M_PI);

                float aspect = (float)height / width;
                float matrix[16] =
                {
                    cosf(angle) * aspect, -sinf(angle), 0, 0,
                    sinf(angle) * aspect,  cosf(angle), 0, 0,
                                       0,            0, 0, 0,
                                       0,            0, 0, 1,
                };
                wgpuQueueWriteBuffer(queue, ubuffer, 0, matrix, sizeof(matrix));
            }

            WGPUSurfaceTexture surfaceTex;
            wgpuSurfaceGetCurrentTexture(surface, &surfaceTex);
            if (surfaceTex.status != WGPUSurfaceGetCurrentTextureStatus_Success)
            {
                FatalError("Cannot acquire next swap chain texture!");
            }

            WGPUTextureViewDescriptor surfaceViewDesc =
            {
                .format = wgpuTextureGetFormat(surfaceTex.texture),
                .dimension = WGPUTextureViewDimension_2D,
                .mipLevelCount = 1,
                .arrayLayerCount = 1,
                .aspect = WGPUTextureAspect_All,
                .usage = WGPUTextureUsage_RenderAttachment,
            };

            WGPUTextureView surfaceView = wgpuTextureCreateView(surfaceTex.texture, &surfaceViewDesc);
            Assert(surfaceView);

            WGPURenderPassDescriptor desc =
            {
                .colorAttachmentCount = 1,
                .colorAttachments = (WGPURenderPassColorAttachment[])
                {
                    // one output to write to, initially cleared with background color
                    {
                        .view = surfaceView,
                        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                        .loadOp = WGPULoadOp_Clear,
                        .storeOp = WGPUStoreOp_Store,
                        .clearValue = { 0.392, 0.584, 0.929, 1.0 }, // r,g,b,a
                    },
                },
            };

            WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, NULL);

            // encode render pass
            WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &desc);
            {
                wgpuRenderPassEncoderSetViewport(pass, 0.f, 0.f, (float)width, (float)height, 0.f, 1.f);

                // draw the triangle using 3 vertices in vertex buffer
                wgpuRenderPassEncoderSetPipeline(pass, pipeline);
                wgpuRenderPassEncoderSetBindGroup(pass, 0, bind_group, 0, NULL);
                wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vbuffer, 0, WGPU_WHOLE_SIZE);
                wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
            }
            wgpuRenderPassEncoderEnd(pass);

            // submit to queue
            WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, NULL);
            wgpuQueueSubmit(queue, 1, &command);

            wgpuCommandBufferRelease(command);
            wgpuCommandEncoderRelease(encoder);
            wgpuTextureViewRelease(surfaceView);

            // present to output
            wgpuSurfacePresent(surface);
        }
    }
}
