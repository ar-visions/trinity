
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
#include <wgpu.h>

static WGPUInstance inst;

#include <import>

#define LOG_PREFIX "[triangle]"

static void handle_request_adapter(WGPURequestAdapterStatus status,
                                   WGPUAdapter adapter, char const *message,
                                   void *userdata) {
  if (status == WGPURequestAdapterStatus_Success) {
    trinity t = userdata;
    t->adapter = adapter;
  } else {
    printf(LOG_PREFIX " request_adapter status=%#.8x message=%s\n", status,
           message);
  }
}
static void handle_request_device(WGPURequestDeviceStatus status,
                                  WGPUDevice device, char const *message,
                                  void *userdata) {
  if (status == WGPURequestDeviceStatus_Success) {
    trinity t = userdata;
    t->device = device;
  } else {
    printf(LOG_PREFIX " request_device status=%#.8x message=%s\n", status,
           message);
  }
}

static void handle_glfw_key(
        GLFWwindow *glfw_window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_R && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        window  w = glfwGetWindowUserPointer(glfw_window);
        trinity t = w->t;
        WGPUGlobalReport report;
        wgpuGenerateReport(t->instance, &report);
    }
}

static void handle_glfw_framebuffer_size(
    GLFWwindow *glfw_window, int width, int height)
{
    if (width == 0 && height == 0)
        return;

    window  w = glfwGetWindowUserPointer(glfw_window);
    trinity t = w->t;
    w->config.width  = width;
    w->config.height = height;
    w->width         = width;
    w->height        = height;
    wgpuSurfaceConfigure(w->surface, &w->config);
}

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

void print_adapter_info(WGPUAdapter adapter) {
  struct WGPUAdapterInfo info = {0};
  wgpuAdapterGetInfo(adapter, &info);
  printf("description:  %s\n", info.description);
  printf("vendor:       %s\n", info.vendor);
  printf("architecture: %s\n", info.architecture);
  printf("device:       %s\n", info.device);
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
    t->surface = wgpuInstanceCreateSurface(
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
                        const WGPUSurfaceDescriptorFromXlibWindow){
                        .chain =
                            (const WGPUChainedStruct){
                                .sType = WGPUSType_SurfaceDescriptorFromXlibWindow,
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
    // create window
    w->window = glfwCreateWindow(w->width, w->height, "triangle [wgpu-native + glfw]", NULL, NULL);
    verify(w->window, "window");

    // set window callbacks
    glfwSetWindowUserPointer(w->window, (void *)&t);
    glfwSetKeyCallback(w->window, handle_glfw_key);
    glfwSetFramebufferSizeCallback(w->window, handle_glfw_framebuffer_size);
    
    // create surface
    window_init_surface(w);
    verify(w->surface, "surface");
    wgpuSurfaceGetCapabilities(w->surface, t->adapter, &w->surface_caps);

    w->config = (const WGPUSurfaceConfiguration){
        .device      = t->device,
        .usage       = WGPUTextureUsage_RenderAttachment,
        .format      = w->surface_caps.formats[0],
        .presentMode = WGPUPresentMode_Fifo,
        .alphaMode   = w->surface_caps.alphaModes[0],
    };

    int width, height;
    glfwGetWindowSize(w->window, &width, &height);
    w->config.width  = width;
    w->config.height = height;

    if (!w->shader) w->shader = shader(t, t, name, str("shader.wgsl"));
    
    w->render = wgpuDeviceCreateRenderPipeline(
        t->device,
        &(const WGPURenderPipelineDescriptor){
            .label = "render",
            .layout = t->layout,
            .vertex =
                (const WGPUVertexState){
                    .module = w->shader,
                    .entryPoint = "vs_main",
                },
            .fragment =
                &(const WGPUFragmentState){
                    .module = w->shader,
                    .entryPoint = "fs_main",
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

    verify(w->render, "render");
}

void window_loop(window w) {
    trinity t = w->t;
    while (!glfwWindowShouldClose(w->window)) {
        glfwPollEvents();

        WGPUSurfaceTexture surface_texture;
        wgpuSurfaceGetCurrentTexture(w->surface, &surface_texture);
        switch (surface_texture.status) {
        case WGPUSurfaceGetCurrentTextureStatus_Success:
            // could check for `surface_texture.suboptimal` here.
            break;
        case WGPUSurfaceGetCurrentTextureStatus_Timeout:
        case WGPUSurfaceGetCurrentTextureStatus_Outdated:
        case WGPUSurfaceGetCurrentTextureStatus_Lost: {
            // skip this frame, and re-configure surface.
            if (surface_texture.texture != NULL) {
                wgpuTextureRelease(surface_texture.texture);
            }
            int width, height;
            glfwGetWindowSize(w->window, &width, &height);
            if (width != 0 && height != 0) {
                w->config.width  = width;
                w->config.height = height;
                w->width         = width;
                w->height        = height;
                wgpuSurfaceConfigure(w->surface, &w->config);
            }
            continue;
        }
        case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
        case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
        case WGPUSurfaceGetCurrentTextureStatus_Force32:
            // fatal error
            fault(LOG_PREFIX " get_current_texture status=%#.8x",
                surface_texture.status);
            break;
        }
        verify(surface_texture.texture, "surface texture");

        WGPUTextureView frame =
            wgpuTextureCreateView(surface_texture.texture, NULL);
        verify(frame, "texture view");

        WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(
            t->device, &(const WGPUCommandEncoderDescriptor){
                            .label = "command_encoder",
                        });
        verify(command_encoder, "command encoder");

        WGPURenderPassEncoder render_pass_encoder =
            wgpuCommandEncoderBeginRenderPass(
                command_encoder,
                &(const WGPURenderPassDescriptor){
                    .label = "render_pass_encoder",
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
                                .label = "command_buffer",
                            });
        verify(command_buffer, "command buffer");

        wgpuQueueSubmit(t->queue, 1, (const WGPUCommandBuffer[]){command_buffer});
        wgpuSurfacePresent(w->surface);

        wgpuCommandBufferRelease(command_buffer);
        wgpuCommandEncoderRelease(command_encoder);
        wgpuTextureViewRelease(frame);
        wgpuTextureRelease(surface_texture.texture);
    }

    wgpuRenderPipelineRelease(w->render);
}

void window_destructor(window w) {
    wgpuSurfaceRelease(w->surface);
    wgpuSurfaceCapabilitiesFreeMembers(w->surface_caps);
    glfwDestroyWindow(w->window);
}



void shader_init(shader s) {
    trinity t    = s->t;
    FILE   *file = fopen(cstring(s->name), "rb");

    verify(file,                          "fopen");
    verify(fseek(file, 0, SEEK_END) == 0, "fseek");
    long   length  = ftell(file);
    verify(length != -1,                  "ftell");
    verify(fseek(file, 0, SEEK_SET) == 0, "fseek");

    char* buf = calloc(1, length + 1);
    fread(buf, 1, length, file);
    buf[length] = 0;

    s->module = wgpuDeviceCreateShaderModule(
        t->device, &(const WGPUShaderModuleDescriptor){
        .label = cstring(s->name),
        .nextInChain =
            (const WGPUChainedStruct *)&(
                const WGPUShaderModuleWGSLDescriptor){
                .chain =
                    (const WGPUChainedStruct){
                        .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                    },
                .code = buf,
            },
    });

    verify(s->module, "shader module");

    fclose(file);
    free  (buf);
}

void shader_destructor(shader s) {
    wgpuShaderModuleRelease(s->module);
}


trinity trinity_init(trinity t) {
    // initialize glfw
    verify (glfwInit(), "glfw init");
    wgpuSetLogCallback(log_callback, NULL);
    wgpuSetLogLevel(WGPULogLevel_Warn);

    t->instance = wgpuCreateInstance(NULL);
    verify(t->instance, "instance");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // create adapter
    wgpuInstanceRequestAdapter(t->instance,
        &(const WGPURequestAdapterOptions) {
            //.compatibleSurface = w->surface,
        },
        handle_request_adapter, t);
    verify(t->adapter, "adapter");

    print_adapter_info(t->adapter);

    wgpuAdapterRequestDevice(t->adapter, NULL, handle_request_device, t);
    verify(t->device, "device");

    t->queue = wgpuDeviceGetQueue(t->device);
    verify(t->queue, "queue");

    t->layout = wgpuDeviceCreatePipelineLayout(
        t->device, &(const WGPUPipelineLayoutDescriptor) {
        .label = "layout",
    });
    verify(t->layout, "layout");
    return 0;
}

void trinity_destructor(trinity t) {
    wgpuPipelineLayoutRelease(t->layout);
    wgpuQueueRelease   (t->queue);
    wgpuDeviceRelease  (t->device);
    wgpuAdapterRelease (t->adapter);
    wgpuInstanceRelease(t->instance);
}

define_class(trinity)
define_class(window)
define_class(shader)