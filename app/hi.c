
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
#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>
#endif

#undef bool
#undef true
#undef false
#undef log
#undef nil
#undef format
#include <A>

#define LOG_PREFIX "[triangle]"


// Vertex structure
typedef struct {
    float position[2]; // X, Y coordinates
} Vertex;

// Define triangle vertices
const Vertex vertices[] = {
    {{-1.0f, -1.0f}}, // Bottom-left
    {{ 0.0f,  1.0f}}, // Top-center
    {{ 1.0f, -1.0f}}, // Bottom-right
};


struct demo {
    WGPUInstance    instance;
    WGPUSurface     surface;
    WGPUAdapter     adapter;
    WGPUDevice      device;
    WGPUSurfaceConfiguration config;
};

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

void print_adapter_info(WGPUAdapter adapter) {
    struct WGPUAdapterInfo info = {0};
    wgpuAdapterGetInfo(adapter, &info);
    printf("description:  %s\n",    info.description.data);
    printf("vendor:       %s\n",    info.vendor.data);
    printf("architecture: %s\n",    info.architecture.data);
    printf("device:       %s\n",    info.device.data);
    printf("backend type: %u\n",    info.backendType);
    printf("adapter type: %u\n",    info.adapterType);
    printf("vendorID:     %x\n",    info.vendorID);
    printf("deviceID:     %x\n",    info.deviceID);
    wgpuAdapterInfoFreeMembers(info);
}

void log_callback(WGPULoggingType level, struct WGPUStringView message, void* userdata) {
    const char* level_str = "";
    switch (level) {
        case WGPULoggingType_Error: level_str = "Error"; break;
        case WGPULoggingType_Warning: level_str = "Warn"; break;
        case WGPULoggingType_Info: level_str = "Info"; break;
        default: level_str = "Unknown"; break;
    }
    fprintf(stdout, "[%s] %s\n", level_str, message.data);
}


WGPUShaderModule load_shader(WGPUDevice device, const char *name) {
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

#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>

// Define selectors as static strings to avoid repeated lookups
static SEL sel_contentView = NULL;
static SEL sel_setWantsLayer = NULL;
static SEL sel_layer = NULL;
static SEL sel_setLayer = NULL;
static SEL sel_frame = NULL;
static SEL sel_setFrame = NULL;

// Initialize selectors once
void init_selectors() {
    if (!sel_contentView) {
        sel_contentView = sel_registerName("contentView");
        sel_setWantsLayer = sel_registerName("setWantsLayer:");
        sel_layer = sel_registerName("layer");
        sel_setLayer = sel_registerName("setLayer:");
        sel_frame = sel_registerName("frame");
        sel_setFrame = sel_registerName("setFrame:");
    }
}

// Helper to make objc_msgSend calls cleaner
id msg_send_id(id self, SEL cmd) {
    return ((id (*)(id, SEL))objc_msgSend)(self, cmd);
}

void msg_send_void_bool(id self, SEL cmd, BOOL arg) {
    ((void (*)(id, SEL, BOOL))objc_msgSend)(self, cmd, arg);
}

void msg_send_void_id(id self, SEL cmd, id arg) {
    ((void (*)(id, SEL, id))objc_msgSend)(self, cmd, arg);
}

void* setup_metal_layer(void* window) {
    init_selectors();
    
    id nsWindow = (id)window;
    id contentView = msg_send_id(nsWindow, sel_contentView);
    
    // Set wantsLayer to YES
    msg_send_void_bool(contentView, sel_setWantsLayer, YES);
    
    // Create metal layer
    Class metalLayerClass = objc_getClass("CAMetalLayer");
    id metalLayer = msg_send_id((id)metalLayerClass, sel_layer);
    
    // Get frame and set it
    CGRect frame = ((CGRect (*)(id, SEL))objc_msgSend)(contentView, sel_frame);
    ((void (*)(id, SEL, CGRect))objc_msgSend)(metalLayer, sel_setFrame, frame);
    
    // Set the layer
    msg_send_void_id(contentView, sel_setLayer, metalLayer);
    
    return metalLayer;
}
#endif

int main(int argc, char *argv[]) {    
    A_start();

    verify(glfwInit(), "glfw init");

    struct demo demo = {0};
    demo.instance = wgpuCreateInstance(NULL);
    verify(demo.instance, "instance");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    //glfwWindowHint(GLFW_COCOA_METAL_LAYER, GLFW_TRUE);
    GLFWwindow *window = glfwCreateWindow(640, 480, "triangle", NULL, NULL);
    verify(window, "window");

    glfwSetWindowUserPointer(window, (void *)&demo);

#ifdef __APPLE__             
    void *ns_window = glfwGetCocoaWindow(window);

    demo.surface = wgpuInstanceCreateSurface(
        demo.instance,
        &(const WGPUSurfaceDescriptor){
            .nextInChain =
                (const WGPUChainedStruct *)&(
                    const WGPUSurfaceDescriptorFromMetalLayer){
                    .chain =
                        (const WGPUChainedStruct){
                            .sType = WGPUSType_SurfaceSourceMetalLayer,
                        },
                    .layer = setup_metal_layer(ns_window),
                },
        });
#else
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
                                .sType = WGPUSType_SurfaceSourceXlibWindow,
                            },
                        .display = x11_display,
                        .window = x11_window,
                    },
            });
    }
#endif
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    wgpuInstanceProcessEvents(demo.instance);
    verify(demo.surface, "surface");

    wgpuInstanceRequestAdapter(demo.instance,
                                &(const WGPURequestAdapterOptions){
                                    .compatibleSurface = demo.surface,
                                },
                                handle_request_adapter, &demo);   verify(demo.adapter, "adapter");
    print_adapter_info(demo.adapter);

    wgpuAdapterRequestDevice    (demo.adapter, NULL, handle_request_device, &demo);   verify(demo.device, "device");
    wgpuDeviceSetLoggingCallback(demo.device, log_callback, NULL);

    WGPUQueue queue = wgpuDeviceGetQueue(demo.device);  verify(queue, "queue");
    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(
        demo.device, &(const WGPUPipelineLayoutDescriptor){});

    WGPUSurfaceCapabilities surface_capabilities = {0};
    wgpuSurfaceGetCapabilities(demo.surface, demo.adapter, &surface_capabilities);

    WGPUShaderModule   shader          = load_shader(demo.device, "shader.wgsl");
    WGPURenderPipeline render_pipeline = wgpuDeviceCreateRenderPipeline(
        demo.device,
        &(const WGPURenderPipelineDescriptor){
            .layout = pipeline_layout,
            .vertex =
                (const WGPUVertexState){
                    .module = shader,
                    .entryPoint = "vs_main",
                    .bufferCount = 1, // We have one vertex buffer
                    .buffers = &(WGPUVertexBufferLayout){
                        .arrayStride = sizeof(Vertex),                // Size of one vertex
                        .stepMode = WGPUVertexStepMode_Vertex,        // Step per vertex
                        .attributeCount = 1,                         // One attribute: position
                        .attributes = &(WGPUVertexAttribute){
                            .shaderLocation = 0,                     // Matches @location(0) in WGSL
                            .offset = offsetof(Vertex, position),     // Offset to the position attribute
                            .format = WGPUVertexFormat_Float32x2,     // vec2<f32> format
                        },
                    },
                },
            .fragment =
                &(const WGPUFragmentState){
                  .module = shader,
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

    //WGPUTextureFormat surface_format = wgpuSurfaceGetPreferredFormat(demo.surface, demo.adapter);
    //verify(surface_format != WGPUTextureFormat_Undefined, "surface format");

    demo.config = (const WGPUSurfaceConfiguration){
        .device = demo.device,
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = surface_capabilities.formats[0],
        .presentMode = WGPUPresentMode_Immediate,  // Changed from Fifo
        .alphaMode = WGPUCompositeAlphaMode_Opaque,
        .width     = width,
        .height    = height
    };

    wgpuSurfaceConfigure(demo.surface, &demo.config);

    /// create vertex buffer of 1 triangle in Vertex format
    int vsize_bytes = sizeof(vertices);
    verify(vsize_bytes == 3 * sizeof(Vertex), "vsize wrong?");
    WGPUBuffer vertex_buffer = wgpuDeviceCreateBuffer(
        demo.device,
        &(WGPUBufferDescriptor){
            .label = "Vertex Buffer",
            .usage = WGPUBufferUsage_Vertex,
            .size = vsize_bytes,
            .mappedAtCreation = true,
        });

    void *mapped_memory = wgpuBufferGetMappedRange(vertex_buffer, 0, sizeof(vertices));
    memcpy(mapped_memory, vertices, sizeof(vertices));
    wgpuBufferUnmap(vertex_buffer);


    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        WGPUSurfaceTexture surface_texture;
        wgpuSurfaceGetCurrentTexture(demo.surface, &surface_texture);
        if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
            printf("Failed to acquire next texture: %d\n", surface_texture.status);
            continue;
        }

        WGPUTextureViewDescriptor view_desc = {
            .format = demo.config.format,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All
        };
        WGPUTextureView    frame   = wgpuTextureCreateView(surface_texture.texture, &view_desc);
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(
            demo.device, &(WGPUCommandEncoderDescriptor){});

        
        WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(
            encoder,
            &(const WGPURenderPassDescriptor) {
                .colorAttachmentCount = 1,
                .colorAttachments =
                    (const WGPURenderPassColorAttachment[]){
                        (const WGPURenderPassColorAttachment){
                            .view       = frame,
                            .loadOp     = WGPULoadOp_Clear,
                            .storeOp    = WGPUStoreOp_Store,
                            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                            .clearValue = (const WGPUColor){ .r = 0.0, .g = 0.0, .b = 0.5, .a = 1.0 }
                        }
                }
            });
        
        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, vertex_buffer, 0, vsize_bytes);
        wgpuRenderPassEncoderSetPipeline(render_pass, render_pipeline);
        wgpuRenderPassEncoderDraw(render_pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(render_pass);
        wgpuRenderPassEncoderRelease(render_pass);
        

        WGPUCommandBuffer commands = wgpuCommandEncoderFinish(
            encoder, &(WGPUCommandBufferDescriptor){});

        wgpuQueueSubmit(queue, 1, &commands);
        wgpuSurfacePresent(demo.surface);
        wgpuTextureViewRelease(frame);
        wgpuCommandBufferRelease(commands);
        wgpuCommandEncoderRelease(encoder);
        wgpuDeviceTick(demo.device);
    }

    wgpuSurfaceCapabilitiesFreeMembers(surface_capabilities);
    wgpuQueueRelease   (queue);
    wgpuDeviceRelease  (demo.device);
    wgpuAdapterRelease (demo.adapter);
    wgpuSurfaceRelease (demo.surface);
    glfwDestroyWindow  (window);
    wgpuInstanceRelease(demo.instance);
    glfwTerminate();
    return 0;
}
