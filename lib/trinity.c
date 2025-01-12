#include <GLFW/glfw3.h>
#if defined(_WIN32)
    #include <windows.h>
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
    #define GLFW_EXPOSE_NATIVE_COCOA
    #include <Foundation/Foundation.h>
    #include <QuartzCore/CAMetalLayer.h>
    #include <objc/message.h>
    #include <objc/runtime.h>
#elif defined(__linux__)
    #define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>
#include <wgpu.h>
#include <import>

#if defined(__APPLE__)
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

static void handle_request_adapter(
    WGPURequestAdapterStatus status, WGPUAdapter adapter,
    const char* message, void *userdata) {
    print("user data = %p", userdata);
    if (status == WGPURequestAdapterStatus_Success) {
        trinity t = userdata;
        t->adapter = adapter;
    } else {
        printf("request_adapter status=%#.8x message=%s\n", status,
            message);
    }
}

static void handle_request_device(
        WGPURequestDeviceStatus status, WGPUDevice device,
        const char* message, void *userdata) {
    if (status == WGPURequestDeviceStatus_Success) {
        trinity t = userdata;
        t->device = device;
    } else {
        printf("request_device status=%#.8x message=%s\n", status,
            message);
    }
}

static void handle_glfw_key(
    GLFWwindow *glfw_window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_R && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
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

void print_adapter_info(WGPUAdapter adapter) {
    struct WGPUAdapterInfo info = {0};
    wgpuAdapterGetInfo(adapter, &info);
    printf("description:  %s\n", (cstr)info.description);
    printf("vendor:       %s\n", (cstr)info.vendor);
    printf("architecture: %s\n", (cstr)info.architecture);
    printf("device:       %s\n", (cstr)info.device);
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
        /*
        id metal_layer = NULL;
        NSWindow *ns_window = glfwGetCocoaWindow(w->window);
        [ns_window.contentView setWantsLayer:YES];
        metal_layer = [CAMetalLayer layer];
        [ns_window.contentView setLayer:metal_layer];
        */
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
                        .layer = setup_metal_layer(ns_window),
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
                                .sType = WGPUSType_SurfaceDescriptorFromXlibWindow
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


void pipeline_init(pipeline p) {
    object  data    = p->read ? p->read : p->read_write; verify(data, "no data");
    bool    is_comp = !p->read;
    A       header  = A_header(data);
    AType   type    = isa(data);
    trinity t       = p->t;
    window  w       = p->w;

    /// data count (vertex-count)
    p->vertex_count = header->count;
    p->total_size = type->size * p->vertex_count;
    
    if (contains(t->buffers, data))
        p->buffer = get(t->buffers, data);
    else {
        WGPUBufferDescriptor desc = {
            .nextInChain = NULL, // No chained extensions
            .label = "buffer", // Debugging label
            .usage = (is_comp ? WGPUBufferUsage_Storage : 0) | WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst, // Usage flags
            .size = p->total_size,  // Size in bytes
            .mappedAtCreation = true,              // Allow CPU access for initialization
        };
        p->buffer = wgpuDeviceCreateBuffer(t->device, &desc);
        if (desc.mappedAtCreation) {
            void* ptr_data = wgpuBufferGetMappedRange(p->buffer, 0, p->total_size);
            memcpy(ptr_data, data, p->total_size);
            wgpuBufferUnmap(p->buffer);
        }
        set(t->buffers, data, p->buffer);
    }

    // bind group layout
    p->bind_layout = wgpuDeviceCreateBindGroupLayout(t->device, &(WGPUBindGroupLayoutDescriptor) {
        .entryCount = p->read ? 0 : 1,
        .entries = &(WGPUBindGroupLayoutEntry) {
            .binding = 0,
            .visibility = is_comp ? WGPUShaderStage_Compute : 
                                    WGPUShaderStage_Vertex,
            .buffer = {
                .type = is_comp ? WGPUBufferBindingType_Storage : 
                                  WGPUBufferBindingType_ReadOnlyStorage,
                .hasDynamicOffset = false,
                .minBindingSize = p->total_size
            }
        }
    });

    // bind group
    p->bind = wgpuDeviceCreateBindGroup(t->device, &(WGPUBindGroupDescriptor) {
        .layout = p->bind_layout,
        .entryCount = p->read ? 0 : 1,
        .entries = &(WGPUBindGroupEntry) {
            .binding = 0,
            .buffer = p->buffer,
            .offset = 0,
            .size = p->total_size
        }
    });
    /// loop for each data group here ^

    /// pipeline layout
    p->layout = wgpuDeviceCreatePipelineLayout(
        t->device, &(const WGPUPipelineLayoutDescriptor) {
        .label = "layout",
        .bindGroupLayouts = &p->bind_layout,
        .bindGroupLayoutCount = p->read ? 0 : 1
    });
    verify(p->layout, "pipeline layout");
    
    if (p->read) {
        /// count attributes by reflecting our A-type data; we introduced an inlay of object for use in vertex structs.. also introduced structs, objects that do not have an f-isa on the end
        int attr_count = 0;
        int offset = 0;
        WGPUVertexAttribute *attrs = calloc(64, sizeof(WGPUVertexAttribute));
        for (int i = 0; i < type->member_count; i++) {
            Member mem = &type->members[i];
            if (mem->member_type == A_TYPE_INLAY || mem->member_type == A_TYPE_PROP) {
                WGPUVertexAttribute *attr = &attrs[attr_count];
                attr->shaderLocation  = attr_count;
                attr->offset          = offset;
                if (mem->type == typeid(v2)) attr->format = WGPUVertexFormat_Float32x2;
                if (mem->type == typeid(v3)) attr->format = WGPUVertexFormat_Float32x3;
                if (mem->type == typeid(v4)) attr->format = WGPUVertexFormat_Float32x4;
                offset += mem->type->size;
                attr_count++;
                break;
            }
        }
        const WGPUVertexBufferLayout vertex_buffer_layout = {
            .arrayStride    = type->size,
            .stepMode       = WGPUVertexStepMode_Vertex,
            .attributes     = attrs,
            .attributeCount = attr_count,
        };
        p->render = wgpuDeviceCreateRenderPipeline(  // <-- this line errors (will describe) 
            t->device,
            &(const WGPURenderPipelineDescriptor){
                .label = "render_pipeline",
                .layout = p->layout,
                .vertex =
                    (const WGPUVertexState){
                        .module = p->shader->module,
                        .entryPoint = "vs_main",
                        .buffers = &vertex_buffer_layout,
                        .bufferCount = 1
                    },
                .fragment =
                    &(const WGPUFragmentState){
                        .module = p->shader->module,
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
                        .topology = WGPUPrimitiveTopology_LineList, // WGPUPrimitiveTopology_TriangleList,
                    },
                .multisample =
                    (const WGPUMultisampleState){
                        .count = 1,
                        .mask = 0xFFFFFFFF,
                    },
            });

        verify(p->render, "render pipeline");
        free(attrs);
    } else {
        // Create Compute Pipeline
        p->compute = wgpuDeviceCreateComputePipeline(
            t->device,
            &(const WGPUComputePipelineDescriptor){
                .label = "compute_pipeline",
                .layout = p->layout,
                .compute =
                    (const WGPUProgrammableStageDescriptor){
                        .module = p->shader->module,
                        .entryPoint = "main",
                    },
            });
        verify(p->compute, "compute pipeline");
    }
}

void pipeline_destructor(pipeline p) {
    wgpuPipelineLayoutRelease(p->layout);
    if (p->render)  wgpuRenderPipelineRelease(p->render);
    if (p->compute) wgpuComputePipelineRelease(p->compute);
}


void window_push(window w, pipeline p) {
    array a = w->pipelines;
    push(a, (object)p);
}

void window_init(window w) {
    trinity t = w->t;
    w->pipelines = array();
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

    w->config = (const WGPUSurfaceConfiguration){
        .device = t->device,
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = w->surface_caps.formats[0],
        .presentMode = WGPUPresentMode_Fifo,
        .alphaMode = w->surface_caps.alphaModes[0],
    };
    int width, height;
    glfwGetWindowSize(w->window, &width, &height);
    w->config.width = width;
    w->config.height = height;
    wgpuSurfaceConfigure(w->surface, &w->config);
}

WGPUTextureView window_frame(window w, bool* skip) {
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(w->surface, &surface_texture);
    *skip = false;
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
        *skip = true;
        break;
    }
    //case WGPUSurfaceGetCurrentTextureStatus_Error:
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
    return frame;
}

void window_loop(window w) {
    trinity t = w->t;

    while (!glfwWindowShouldClose(w->window)) {
        glfwPollEvents();
        bool skip = false;
        WGPUTextureView frame = window_frame(w, &skip);
        if (!frame) {
            if (skip) continue; else break;
        }
        WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(
            t->device, &(const WGPUCommandEncoderDescriptor){
                .label = "command_encoder",
            });

        each(w->pipelines, pipeline, p) {
            if (p->compute) {
                // Compute pipeline execution
                WGPUComputePassEncoder compute_pass = wgpuCommandEncoderBeginComputePass(
                    command_encoder, &(const WGPUComputePassDescriptor){
                        .label = "compute_pass",
                    });
                wgpuComputePassEncoderSetPipeline(compute_pass, p->compute);
                wgpuComputePassEncoderSetBindGroup(compute_pass, 0, p->bind, 0, NULL);
                wgpuComputePassEncoderDispatchWorkgroups(compute_pass, p->vertex_count, 1, 1);
                wgpuComputePassEncoderEnd(compute_pass);
                wgpuComputePassEncoderRelease(compute_pass);
            }
        }

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
                                .view       = frame,
                                .loadOp     = WGPULoadOp_Clear,
                                .storeOp    = WGPUStoreOp_Store,
                                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                                .clearValue = (const WGPUColor) { 0.2, 0.0, 0.5, 1.0 },
                            },
                        },
                });
        verify(render_pass_encoder, "render_pass_encoder");

        /// run the pipelines; here we will need to know when to sync data, or must we also collect identity above when we register?
        each(w->pipelines, pipeline, p) {
            if (p->render) {
                wgpuRenderPassEncoderSetVertexBuffer(render_pass_encoder, 0, p->buffer, 0, WGPU_WHOLE_SIZE);
                wgpuRenderPassEncoderSetPipeline(render_pass_encoder, p->render);
                wgpuRenderPassEncoderDraw(render_pass_encoder, p->vertex_count, 1, 0, 0);
            }
        }

        wgpuRenderPassEncoderEnd(render_pass_encoder);
        wgpuRenderPassEncoderRelease(render_pass_encoder);
        WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(
            command_encoder, &(const WGPUCommandBufferDescriptor){
                .label = "command_buffer"
            });
        verify(command_buffer, "command buffer");
        wgpuQueueSubmit(t->queue, 1, (const WGPUCommandBuffer[]){command_buffer});
        wgpuSurfacePresent(w->surface);
        wgpuCommandBufferRelease(command_buffer);
        wgpuCommandEncoderRelease(command_encoder);
        wgpuTextureViewRelease(frame);
        //wgpuTextureRelease(surface_texture.texture);

        // synchronize GPU data back to CPU where needed
        if (false)
        pairs (t->buffers, i) {
            WGPUBuffer buffer = i->value;
            object     data   = i->key;
            AType      type   = isa(data);
            A          header = A_header(data);
            int total_size = header->count * type->size;
            void* ptr_data = wgpuBufferGetMappedRange(buffer, 0, total_size);
            memcpy(data, ptr_data, total_size);
            wgpuBufferUnmap(buffer);
        }
    }
}

void window_destructor(window w) {
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
        .label = s->name->chars,
        .nextInChain =
            (const WGPUChainedStruct *)&(
                const WGPUShaderModuleWGSLDescriptor){
                .chain =
                    (const WGPUChainedStruct){
                        .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                    },
                .code = buf
            },
    });
    fclose (file);
    //free   (buf);
}


void shader_destructor(shader s) {
    wgpuShaderModuleRelease(s->module);
}


static void trinity_log(WGPULogLevel level, const char* message, void *userdata) {
    char *level_str;
    switch (level) {
    case WGPULogLevel_Error: level_str = "error"; break;
    case WGPULogLevel_Warn:  level_str = "warn";  break;
    case WGPULogLevel_Info:  level_str = "info";  break;
    case WGPULogLevel_Debug: level_str = "debug"; break;
    case WGPULogLevel_Trace: level_str = "trace"; break;
    default:
        level_str = "unknown_level";
    }
    print("[wgpu] [%s] %s", level_str, message);
}


void trinity_init(trinity t) {
    /// init / instance
    verify (glfwInit(), "glfw init");
    t->instance = wgpuCreateInstance(NULL);
    verify(t->instance, "instance");

    /// adapter / device
    wgpuInstanceRequestAdapter(t->instance,
        &(const WGPURequestAdapterOptions){ },
        handle_request_adapter, t);
    verify(t->adapter, "adapter");
    wgpuAdapterRequestDevice(t->adapter, NULL, handle_request_device, t);
    verify(t->device, "device");

    /// queue
    t->queue = wgpuDeviceGetQueue(t->device);
    verify(t->queue, "queue");

    /// buffer management between pipeline
    t->buffers = map();

    /// logging
    wgpuSetLogCallback(trinity_log, NULL);
    wgpuSetLogLevel(WGPULogLevel_Warn);
}

void trinity_destructor(trinity t) {
    wgpuQueueRelease   (t->queue);
    wgpuDeviceRelease  (t->device);
    wgpuAdapterRelease (t->adapter);
    wgpuInstanceRelease(t->instance);
    glfwTerminate();
}

define_class(v2)
define_class(v3)
define_class(v4)

define_class(trinity)
define_class(shader)
define_class(pipeline)
define_class(window)

define_class(vertex)
define_class(particle)
