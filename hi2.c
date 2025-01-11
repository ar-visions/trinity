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
                                   WGPUAdapter adapter, const char* message,
                                   void *userdata) {
  if (status == WGPURequestAdapterStatus_Success) {
    struct demo *demo = userdata;
    demo->adapter = adapter;
  } else {
    printf(LOG_PREFIX " request_adapter status=%#.8x message=%s\n", status,
           message);
  }
}
static void handle_request_device(WGPURequestDeviceStatus status,
                                  WGPUDevice device, const char* message,
                                  void *userdata) {
  if (status == WGPURequestDeviceStatus_Success) {
    struct demo *demo = userdata;
    demo->device = device;
  } else {
    printf(LOG_PREFIX " request_device status=%#.8x message=%s\n", status,
           message);
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
  printf("description: %s\n", info.description);
  printf("vendor: %s\n", info.vendor);
  printf("architecture: %s\n", info.architecture);
  printf("device: %s\n", info.device);
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
                          const WGPUShaderModuleWGSLDescriptor){
                          .chain =
                              (const WGPUChainedStruct){
                                  .sType = WGPUSType_ShaderModuleWGSLDescriptor,
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
    verify(glfwInit(), "glfw init");

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

    if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
        Display *x11_display = glfwGetX11Display();
        Window x11_window = glfwGetX11Window(window);
        demo.surface = wgpuInstanceCreateSurface(
            demo.instance,
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

    wgpuInstanceProcessEvents(demo.instance);
    
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

    WGPUSurfaceCapabilities surface_capabilities = {0};
    wgpuSurfaceGetCapabilities(demo.surface, demo.adapter, &surface_capabilities);

    demo.config = (const WGPUSurfaceConfiguration){
        .device = demo.device,
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = surface_capabilities.formats[0],
        .presentMode = WGPUPresentMode_Immediate,  // Changed from Fifo
        .alphaMode = surface_capabilities.alphaModes[0],
    };

    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        demo.config.width = width;
        demo.config.height = height;
    }

    printf("Using format: %d\n", demo.config.format);  // Debug print
    wgpuSurfaceConfigure(demo.surface, &demo.config);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        WGPUSurfaceTexture surface_texture;
        wgpuSurfaceGetCurrentTexture(demo.surface, &surface_texture);
        if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
            printf("Failed to acquire next texture: %d\n", surface_texture.status);
            continue;
        }

        WGPUTextureView frame = wgpuTextureCreateView(surface_texture.texture, null);
        
        if (!frame) {
            printf("Failed to create texture view\n");
            wgpuTextureRelease(surface_texture.texture);
            continue;
        }

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(
            demo.device, &(WGPUCommandEncoderDescriptor){
                .label = "Command Encoder",
            });

        WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(
            encoder,
            &(WGPURenderPassDescriptor){
                .colorAttachmentCount = 1,
                .colorAttachments = &(WGPURenderPassColorAttachment){
                    .view = frame,
                    .loadOp = WGPULoadOp_Clear,
                    .storeOp = WGPUStoreOp_Store,
                    .clearValue = (WGPUColor){
                        .r = 1.0f,  // Changed to bright white for testing
                        .g = 1.0f,
                        .b = 1.0f,
                        .a = 1.0f,
                    },
                },
            });

        wgpuRenderPassEncoderEnd(render_pass);
        
        WGPUCommandBuffer commands = wgpuCommandEncoderFinish(
            encoder, &(WGPUCommandBufferDescriptor){
                .label = "Command Buffer",
            });

        wgpuQueueSubmit(queue, 1, &commands);
        wgpuSurfacePresent(demo.surface);

        // Cleanup
        wgpuCommandBufferRelease(commands);
        wgpuRenderPassEncoderRelease(render_pass);
        wgpuCommandEncoderRelease(encoder);
        wgpuTextureViewRelease(frame);
        wgpuTextureRelease(surface_texture.texture);
    }

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
