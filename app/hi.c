//#include <import>
/*
int main(int argc, char *argv[]) {
    A_start();
    trinity t = trinity();
    shader  s = shader(t, t, name, str("shader.wgsl"));
    window  w = window(t, t, shader, s, width, 800, height, 600);
    loop   (w);
    return  0;
}
*/


#include <A>
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

#define LOG_PREFIX "[triangle]"

struct demo {
  WGPUInstance instance;
  WGPUSurface surface;
  WGPUAdapter adapter;
  WGPUDevice device;
  WGPUSurfaceConfiguration config;
};

// WGPURequestAdapterStatus status, WGPUAdapter adapter, struct WGPUStringView message, void * userdata

static void handle_request_adapter(WGPURequestAdapterStatus status,
                                   WGPUAdapter adapter, struct WGPUStringView message,
                                   void *userdata) {
  if (status == WGPURequestAdapterStatus_Success) {
    struct demo *demo = userdata;
    demo->adapter = adapter;
  } else {
    printf(LOG_PREFIX " request_adapter status=%#.8x message=%s\n", status,
           message.data);
  }
}
static void handle_request_device(WGPURequestDeviceStatus status,
                                  WGPUDevice device, struct WGPUStringView message,
                                  void *userdata) {
  if (status == WGPURequestDeviceStatus_Success) {
    struct demo *demo = userdata;
    demo->device = device;
  } else {
    printf(LOG_PREFIX " request_device status=%#.8x message=%s\n", status,
           message.data);
  }
}
static void handle_glfw_key(GLFWwindow *window, int key, int scancode,
                            int action, int mods) {
  if (key == GLFW_KEY_R && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    struct demo *demo = glfwGetWindowUserPointer(window);
    if (!demo || !demo->instance)
      return;
  }
}
static void handle_glfw_framebuffer_size(GLFWwindow *window, int width,
                                         int height) {
  if (width == 0 && height == 0) {
    return;
  }

  struct demo *demo = glfwGetWindowUserPointer(window);
  if (!demo)
    return;

  demo->config.width = width;
  demo->config.height = height;

  wgpuSurfaceConfigure(demo->surface, &demo->config);
}

void print_adapter_info(WGPUAdapter adapter) {
  struct WGPUAdapterInfo info = {0};
  wgpuAdapterGetInfo(adapter, &info);
  printf("description: %s\n", info.description.data);
  printf("vendor: %s\n", info.vendor.data);
  printf("architecture: %s\n", info.architecture.data);
  printf("device: %s\n", info.device.data);
  printf("backend type: %u\n", info.backendType);
  printf("adapter type: %u\n", info.adapterType);
  printf("vendorID: %x\n", info.vendorID);
  printf("deviceID: %x\n", info.deviceID);
  wgpuAdapterInfoFreeMembers(info);
}


WGPUShaderModule frmwrk_load_shader_module(WGPUDevice device,
                                           const char *name) {
  FILE *file = NULL;
  char *buf = NULL;
  WGPUShaderModule shader_module = NULL;

  file = fopen(name, "rb");
  if (!file) {
    perror("fopen");
    goto cleanup;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    perror("fseek");
    goto cleanup;
  }
  long length = ftell(file);
  if (length == -1) {
    perror("ftell");
    goto cleanup;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    perror("fseek");
    goto cleanup;
  }

  buf = calloc(1, length + 1);
  fread(buf, 1, length, file);
  buf[length] = 0;

  shader_module = wgpuDeviceCreateShaderModule(
      device, &(const WGPUShaderModuleDescriptor){
                  .label = name,
                  .nextInChain =
                      (const WGPUChainedStruct *)&(
                          const WGPUShaderSourceWGSL){
                          .chain =
                              (const WGPUChainedStruct){
                                  .sType = WGPUSType_ShaderSourceWGSL,
                              },
                          .code = buf,
                      },
              });

cleanup:
  if (file)
    fclose(file);
  if (buf)
    free(buf);
  return shader_module;
}

void log_callback(WGPULoggingType level, char const* message, void* userdata) {
    const char* level_str = "";
    switch (level) {
        case WGPULoggingType_Error: level_str = "Error"; break;
        case WGPULoggingType_Warning: level_str = "Warn"; break;
        case WGPULoggingType_Info: level_str = "Info"; break;
        default: level_str = "Unknown"; break;
    }
    fprintf(stderr, "[%s] %s\n", level_str, message);
}

int main(int argc, char *argv[]) {
    A_start();
    verify (glfwInit(), "glfw init");

    struct demo demo = {0};
    demo.instance = wgpuCreateInstance(NULL);
    verify(demo.instance, "instance");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window =
        glfwCreateWindow(640, 480, "triangle [wgpu-native + glfw]", NULL, NULL);
    verify(window, "window");

    glfwSetWindowUserPointer(window, (void *)&demo);
    glfwSetKeyCallback(window, handle_glfw_key);
    glfwSetFramebufferSizeCallback(window, handle_glfw_framebuffer_size);

#if defined(GLFW_EXPOSE_NATIVE_X11)
    if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
        Display *x11_display = glfwGetX11Display();
        Window x11_window = glfwGetX11Window(window);
        demo.surface = wgpuInstanceCreateSurface(
            demo.instance,
            &(const WGPUSurfaceDescriptor){
                .nextInChain =
                    (const WGPUChainedStruct *)&(
                        const WGPUSurfaceSourceXlibWindow){
                        .chain =
                            (const WGPUChainedStruct){
                                .sType = WGPUSType_SurfaceSourceXlibWindow,
                            },
                        .display = x11_display,
                        .window = x11_window,
                    },
            });
    }
#elif defined(GLFW_EXPOSE_NATIVE_WAYLAND)
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        struct wl_display *wayland_display = glfwGetWaylandDisplay();
        struct wl_surface *wayland_surface = glfwGetWaylandWindow(window);
        demo.surface = wgpuInstanceCreateSurface(
            demo.instance,
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
        HWND hwnd = glfwGetWin32Window(window);
        HINSTANCE hinstance = GetModuleHandle(NULL);
        demo.surface = wgpuInstanceCreateSurface(
            demo.instance,
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

    verify(demo.surface, "surface");

    wgpuInstanceRequestAdapter(demo.instance,
                                &(const WGPURequestAdapterOptions){
                                    .compatibleSurface = demo.surface,
                                },
                                handle_request_adapter, &demo);
    verify(demo.adapter, "adapter");

    print_adapter_info(demo.adapter);

    wgpuAdapterRequestDevice(demo.adapter, NULL, handle_request_device, &demo);
    verify(demo.device, "device");

    wgpuDeviceSetLoggingCallback(demo.device, log_callback, NULL);

    WGPUQueue queue = wgpuDeviceGetQueue(demo.device);
    verify(queue, "queue");

    WGPUShaderModule shader_module =
        frmwrk_load_shader_module(demo.device, "shader.wgsl");
    verify(shader_module, "shader module");

    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(
        demo.device, &(const WGPUPipelineLayoutDescriptor){
                        .label = "pipeline_layout",
                    });
    verify(pipeline_layout, "pipeline layout");

    WGPUSurfaceCapabilities surface_capabilities = {0};
    wgpuSurfaceGetCapabilities(demo.surface, demo.adapter, &surface_capabilities);

    WGPURenderPipeline render_pipeline = wgpuDeviceCreateRenderPipeline(
        demo.device,
        &(const WGPURenderPipelineDescriptor){
            .label = "render_pipeline",
            .layout = pipeline_layout,
            .vertex =
                (const WGPUVertexState){
                    .module = shader_module,
                    .entryPoint = "vs_main",
                },
            .fragment =
                &(const WGPUFragmentState){
                    .module = shader_module,
                    .entryPoint = "fs_main",
                    .targetCount = 1,
                    .targets =
                        (const WGPUColorTargetState[]){
                            (const WGPUColorTargetState){
                                .format = surface_capabilities.formats[0],
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

    verify(render_pipeline, "render pipeline");

    demo.config = (const WGPUSurfaceConfiguration){
        .device = demo.device,
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = surface_capabilities.formats[0],
        .presentMode = WGPUPresentMode_Fifo,
        .alphaMode = surface_capabilities.alphaModes[0],
    };

    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        demo.config.width = width;
        demo.config.height = height;
    }

    wgpuSurfaceConfigure(demo.surface, &demo.config);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        WGPUSurfaceTexture surface_texture;
        wgpuSurfaceGetCurrentTexture(demo.surface, &surface_texture);
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
            glfwGetWindowSize(window, &width, &height);
            if (width != 0 && height != 0) {
                demo.config.width = width;
                demo.config.height = height;
                wgpuSurfaceConfigure(demo.surface, &demo.config);
            }
            continue;
        }
        case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
        case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
        case WGPUSurfaceGetCurrentTextureStatus_Force32:
            // Fatal error
            fault(LOG_PREFIX " get_current_texture status=%#.8x",
                surface_texture.status);
            break;
        default:
            exit(0);
            break;
        }
        verify(surface_texture.texture, "surface texture");

        WGPUTextureView frame =
            wgpuTextureCreateView(surface_texture.texture, NULL);
        verify(frame, "texture view");

        WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(
            demo.device, &(const WGPUCommandEncoderDescriptor){
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

        wgpuRenderPassEncoderSetPipeline(render_pass_encoder, render_pipeline);
        wgpuRenderPassEncoderDraw(render_pass_encoder, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(render_pass_encoder);
        wgpuRenderPassEncoderRelease(render_pass_encoder);

        WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(
            command_encoder, &(const WGPUCommandBufferDescriptor){
                                .label = "command_buffer",
                            });
        verify(command_buffer, "command buffer");

        wgpuQueueSubmit(queue, 1, (const WGPUCommandBuffer[]){command_buffer});
        wgpuSurfacePresent(demo.surface);

        wgpuCommandBufferRelease(command_buffer);
        wgpuCommandEncoderRelease(command_encoder);
        wgpuTextureViewRelease(frame);
        wgpuTextureRelease(surface_texture.texture);
    }

    wgpuRenderPipelineRelease(render_pipeline);
    wgpuPipelineLayoutRelease(pipeline_layout);
    wgpuShaderModuleRelease(shader_module);
    wgpuSurfaceCapabilitiesFreeMembers(surface_capabilities);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(demo.device);
    wgpuAdapterRelease(demo.adapter);
    wgpuSurfaceRelease(demo.surface);
    glfwDestroyWindow(window);
    wgpuInstanceRelease(demo.instance);
    glfwTerminate();
    return 0;
}
