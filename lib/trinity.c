/*
import something, something-else

# this file is simply a module and that module is a class, and this file has public and private members
# its more like reflective modular programming and probably more exciting to teach
# yes, you have file system expression here so you cannot put anything you want in code to the end of any model combination
# thats a limiting factor, but, its also about as clear as we can muster for navigating
# and its not functionally restrictive but more a model language

static int something : 20 # mutable static, in public access
       int a-public  = 24 # constant instance member on this mod

intern int a-priv    : 2  # instance member that starts at 2

# modules are instanced the same as classes.. thats just another word for them
# we say mod because its in a singular translation unit and its coupled to a data type
# this is more debuggable and enumerable than traditional struct / class / enum / etc
# i do think we try an enum module next
# the very cool thing is everything in the file is about that type, and nothing else

public string cast [
    return 'anything'
]

public void a-function[ string arg, int a ] [
    print[ 'this is easier, so we can do {this}...' ]
]
*/

#include <GLFW/glfw3.h>
#if defined(_WIN32)
    #include <windows.h>
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
    #define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
    #define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>
#include <dawn/webgpu.h>
#include <import>

#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wcompare-distinct-pointer-types"
#pragma GCC diagnostic error "-Wincompatible-pointer-types"
#pragma GCC diagnostic error "-Wincompatible-library-redeclaration"
#pragma GCC diagnostic error "-Wwrite-strings"
#pragma GCC diagnostic error "-Wint-conversion"
#pragma GCC diagnostic error "-Wall"

static void handle_device_error(
    WGPUErrorType type, struct WGPUStringView message, void * userdata) {
    printf("device error: message=%s\n", message.data);
}

static void handle_request_adapter(
    WGPURequestAdapterStatus status, WGPUAdapter adapter,
    WGPUStringView message, void *userdata) {
    print("user data = %p", userdata);
    if (status == WGPURequestAdapterStatus_Success) {
        trinity t = userdata;
        t->adapter = adapter;
    } else {
        printf("request_adapter status=%#.8x message=%s\n", status,
            message.data);
    }
}

static void handle_request_device(
        WGPURequestDeviceStatus status, WGPUDevice device,
        WGPUStringView message, void *userdata) {
    if (status == WGPURequestDeviceStatus_Success) {
        trinity t = userdata;
        t->device = device;
    } else {
        printf("request_device status=%#.8x message=%s\n", status,
            message.data);
    }
}

static void handle_glfw_key(
    GLFWwindow *glfw_window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_R && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        //window w = glfwGetWindowUserPointer(glfw_window);
        //WGPUGlobalReport report;
        //wgpuGenerateReport(w->t->instance, &report);
    }
}

static void handle_glfw_framebuffer_size(
    GLFWwindow *glfw_window, int width, int height) {
    if (width == 0 && height == 0) return;
    window w = glfwGetWindowUserPointer(glfw_window);
    w->config.width  = width;
    w->config.height = height;
    wgpuSurfaceConfigure(w->surface, &w->config);
}

/*
static void log_callback(WGPULogLevel level, char const *message, void *userdata) {
    char *level_str;
    switch (level) {
    case WGPULogLevel_Error:
        level_str = "error";
        break;
    case WGPULogLevel_Warn:
        level_str = "warn";
        break;
    case WGPULogLevel_Info:
        level_str = "info";
        break;
    case WGPULogLevel_Debug:
        level_str = "debug";
        break;
    case WGPULogLevel_Trace:
        level_str = "trace";
        break;
    default:
        level_str = "unknown_level";
    }
    error("[wgpu] [%s] %s", level_str, message);
}
*/

void print_adapter_info(WGPUAdapter adapter) {
    struct WGPUAdapterInfo info = {0};
    wgpuAdapterGetInfo(adapter, &info);
    printf("description:  %s\n", (cstr)info.description.data);
    printf("vendor:       %s\n", (cstr)info.vendor.data);
    printf("architecture: %s\n", (cstr)info.architecture.data);
    printf("device:       %s\n", (cstr)info.device.data);
    printf("backend type: %u\n", info.backendType);
    printf("adapter type: %u\n", info.adapterType);
    printf("vendorID:     %x\n", info.vendorID);
    printf("deviceID:     %x\n", info.deviceID);
    wgpuAdapterInfoFreeMembers(info);
}

void window_init_surface(window w) {
    trinity t = w->t;
#if defined(GLFW_EXPOSE_NATIVE_COCOA)
    {
        id metal_layer = NULL;
        NSWindow *ns_window = glfwGetCocoaWindow(w->window);
        [ns_window.contentView setWantsLayer:YES];
        metal_layer = [CAMetalLayer layer];
        [ns_window.contentView setLayer:metal_layer];
        w->surface = wgpuInstanceCreateSurface(
            t->instance,
            &(const WGPUSurfaceDescriptor){
                .nextInChain =
                    (const WGPUChainedStruct *)&(
                        const WGPUSurfaceDescriptorFromMetalLayer){
                        .chain =
                            (const WGPUChainedStruct){
                                .sType = WGPUSType_SurfaceDescriptorFromMetalLayer,
                            },
                        .layer = metal_layer,
                    },
            });
    }
#elif defined(GLFW_EXPOSE_NATIVE_X11)
    if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
        Display *x11_display = glfwGetX11Display();
        Window x11_window = glfwGetX11Window(w->window);
        w->surface = wgpuInstanceCreateSurface(
            t->instance,
            &(const WGPUSurfaceDescriptor){
                .nextInChain =
                    (const WGPUChainedStruct *)&(
                        const WGPUSurfaceSourceXlibWindow){
                        .chain =
                            (const WGPUChainedStruct){
                                .sType = WGPUSType_SurfaceSourceXlibWindow
                            },
                        .display = x11_display,
                        .window = x11_window,
                    },
            });
    }
#elif defined(GLFW_EXPOSE_NATIVE_WAYLAND)
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        struct wl_display *wayland_display = glfwGetWaylandDisplay();
        struct wl_surface *wayland_surface = glfwGetWaylandWindow(w->window);
        w->surface = wgpuInstanceCreateSurface(
            t->instance,
            &(const WGPUSurfaceDescriptor){
                .nextInChain =
                    (const WGPUChainedStruct *)&(
                        const WGPUSurfaceDescriptorFromWaylandSurface){
                        .chain =
                            (const WGPUChainedStruct){
                                .sType =
                                    WGPUSType_SurfaceDescriptorFromWaylandSurface,
                            },
                        .display = wayland_display,
                        .surface = wayland_surface,
                    },
            });
    }
#elif defined(GLFW_EXPOSE_NATIVE_WIN32)
    {
        HWND hwnd = glfwGetWin32Window(w->window);
        HINSTANCE hinstance = GetModuleHandle(NULL);
        w->surface = wgpuInstanceCreateSurface(
            t->instance,
            &(const WGPUSurfaceDescriptor){
                .nextInChain =
                    (const WGPUChainedStruct *)&(
                        const WGPUSurfaceDescriptorFromWindowsHWND){
                        .chain =
                            (const WGPUChainedStruct){
                                .sType = WGPUSType_SurfaceDescriptorFromWindowsHWND,
                            },
                        .hinstance = hinstance,
                        .hwnd = hwnd,
                    },
            });
    }
#else
#error "Unsupported GLFW native platform"
#endif
}

void window_init(window w) {
    trinity t = w->t;
    shader  s = w->shader;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    w->window = glfwCreateWindow(
        w->width, w->height, "triangle [wgpu-native + glfw]", NULL, NULL);
    verify(w->window, "window");
    glfwSetWindowUserPointer(w->window, (void *)w);
    glfwSetKeyCallback(w->window, handle_glfw_key);
    glfwSetFramebufferSizeCallback(w->window, handle_glfw_framebuffer_size);
    window_init_surface(w);
    verify(w->surface, "surface");
    wgpuSurfaceGetCapabilities(w->surface, t->adapter, &w->surface_caps);

    w->render = wgpuDeviceCreateRenderPipeline(
        t->device,
        &(const WGPURenderPipelineDescriptor){
            .label = (WGPUStringView) { "render_pipeline", 15 },
            .layout = t->layout,
            .vertex =
                (const WGPUVertexState){
                    .module = s->module,
                    .entryPoint = (WGPUStringView) { "vs_main", 7 },
                },
            .fragment =
                &(const WGPUFragmentState){
                    .module = s->module,
                    .entryPoint = (WGPUStringView) { "fs_main", 7 },
                    .targetCount = 1,
                    .targets =
                        (const WGPUColorTargetState[]){
                            (const WGPUColorTargetState){
                                .format = w->surface_caps.formats[0],
                                .writeMask = WGPUColorWriteMask_All,
                            },
                        },
                },
            .primitive =
                (const WGPUPrimitiveState){
                    .topology = WGPUPrimitiveTopology_TriangleList,
                },
            .multisample =
                (const WGPUMultisampleState){
                    .count = 1,
                    .mask = 0xFFFFFFFF,
                },
        });

    verify(w->render, "render pipeline");

    w->config = (const WGPUSurfaceConfiguration){
        .device = t->device,
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = w->surface_caps.formats[0],
        .presentMode = WGPUPresentMode_Fifo,
        .alphaMode = w->surface_caps.alphaModes[0],
    };

    {
        int width, height;
        glfwGetWindowSize(w->window, &width, &height);
        w->config.width = width;
        w->config.height = height;
    }

    wgpuSurfaceConfigure(w->surface, &w->config);
}

void window_loop(window w) {
    trinity t = w->t;
    while (!glfwWindowShouldClose(w->window)) {
        glfwPollEvents();

        WGPUSurfaceTexture surface_texture;
        wgpuSurfaceGetCurrentTexture(w->surface, &surface_texture);
        switch (surface_texture.status) {
        case WGPUSurfaceGetCurrentTextureStatus_Success:
            // All good, could check for `surface_texture.suboptimal` here.
            break;
        case WGPUSurfaceGetCurrentTextureStatus_Timeout:
        case WGPUSurfaceGetCurrentTextureStatus_Outdated:
        case WGPUSurfaceGetCurrentTextureStatus_Lost: {
            // Skip this frame, and re-configure surface.
            if (surface_texture.texture != NULL) {
                wgpuTextureRelease(surface_texture.texture);
            }
            int width, height;
            glfwGetWindowSize(w->window, &width, &height);
            if (width != 0 && height != 0) {
                w->config.width = width;
                w->config.height = height;
                wgpuSurfaceConfigure(w->surface, &w->config);
            }
            continue;
        }
        case WGPUSurfaceGetCurrentTextureStatus_Error:
        case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
        case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
        case WGPUSurfaceGetCurrentTextureStatus_Force32:
            // Fatal error
            fault("get_current_texture status=%#.8x",
                surface_texture.status);
            break;
        }
        verify(surface_texture.texture, "surface texture");

        WGPUTextureView frame =
            wgpuTextureCreateView(surface_texture.texture, NULL);
        verify(frame, "texture view");

        WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(
            t->device, &(const WGPUCommandEncoderDescriptor){
                .label = (WGPUStringView) { "command_encoder", 15 },
            });
        verify(command_encoder, "command encoder");

        WGPURenderPassEncoder render_pass_encoder =
            wgpuCommandEncoderBeginRenderPass(
                command_encoder,
                &(const WGPURenderPassDescriptor){
                    .label = (WGPUStringView) { "render_pass_encoder", 19 },
                    .colorAttachmentCount = 1,
                    .colorAttachments =
                        (const WGPURenderPassColorAttachment[]){
                            (const WGPURenderPassColorAttachment){
                                .view = frame,
                                .loadOp = WGPULoadOp_Clear,
                                .storeOp = WGPUStoreOp_Store,
                                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                                .clearValue =
                                    (const WGPUColor){
                                        .r = 0.2,
                                        .g = 0.0,
                                        .b = 0.5,
                                        .a = 1.0,
                                    },
                            },
                        },
                });
        verify(render_pass_encoder, "render_pass_encoder");
        wgpuRenderPassEncoderSetPipeline(render_pass_encoder, w->render);
        wgpuRenderPassEncoderDraw(render_pass_encoder, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(render_pass_encoder);
        wgpuRenderPassEncoderRelease(render_pass_encoder);
        WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(
            command_encoder, &(const WGPUCommandBufferDescriptor){
                .label = (WGPUStringView) { "command_buffer", 14 }
            });
        verify(command_buffer, "command buffer");
        wgpuQueueSubmit(t->queue, 1, (const WGPUCommandBuffer[]){command_buffer});
        wgpuSurfacePresent(w->surface);
        wgpuCommandBufferRelease(command_buffer);
        wgpuCommandEncoderRelease(command_encoder);
        wgpuTextureViewRelease(frame);
        wgpuTextureRelease(surface_texture.texture);
    }
}

void window_destructor(window w) {
    wgpuRenderPipelineRelease(w->render);
    wgpuSurfaceCapabilitiesFreeMembers(w->surface_caps);
    wgpuSurfaceRelease (w->surface);
    glfwDestroyWindow  (w->window);
}

void shader_init(shader s) {
    FILE* file = fopen(s->name->chars, "rb");
    verify (file, "file");
    verify (fseek(file, 0, SEEK_END) == 0, "fseek");
    long length = ftell(file);
    verify (length != -1, "length");
    verify (fseek(file, 0, SEEK_SET) == 0, "fseek2");
    char* buf = calloc(1, length + 1);
    fread  (buf, 1, length, file);
    buf[length] = 0;
    s->module = wgpuDeviceCreateShaderModule(
    s->t->device, &(const WGPUShaderModuleDescriptor){
        .label = (WGPUStringView) { s->name->chars, s->name->len },
        .nextInChain =
            (const WGPUChainedStruct *)&(
                const WGPUShaderSourceWGSL){
                .chain =
                    (const WGPUChainedStruct){
                        .sType = WGPUSType_ShaderSourceWGSL,
                    },
                .code = (WGPUStringView) { buf, length }
            },
    });
    fclose (file);
    free   (buf);
}

void shader_destructor(shader s) {
    wgpuShaderModuleRelease(s->module);
}

void trinity_init(trinity t) {
    verify (glfwInit(), "glfw init");
    t->instance = wgpuCreateInstance(NULL);

    verify(t->instance, "instance");

    wgpuInstanceRequestAdapter(t->instance,
        &(const WGPURequestAdapterOptions){ },
        handle_request_adapter, t);
    verify(t->adapter, "adapter");

    wgpuAdapterRequestDevice(t->adapter, NULL, handle_request_device, t);
    verify(t->device, "device");
    wgpuDeviceSetUncapturedErrorCallback(t->device, handle_device_error, t);

    t->queue = wgpuDeviceGetQueue(t->device);
    verify(t->queue, "queue");

    t->layout = wgpuDeviceCreatePipelineLayout(
        t->device, &(const WGPUPipelineLayoutDescriptor) {
        .label = (WGPUStringView) { "layout", 6 }
    });
    verify(t->layout, "pipeline layout");
}

void trinity_destructor(trinity t) {
    wgpuPipelineLayoutRelease(t->layout);
    wgpuQueueRelease   (t->queue);
    wgpuDeviceRelease  (t->device);
    wgpuAdapterRelease (t->adapter);
    wgpuInstanceRelease(t->instance);
    glfwTerminate();
}

#pragma GCC diagnostic pop

define_class(trinity)
define_class(window)
define_class(shader)
