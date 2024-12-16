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

#if 0

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

/*static void handle_device_error(
    WGPUErrorType type, struct WGPUStringView message, void * userdata) {
    printf("device error: message=%s\n", message.data);
}*/

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
    //wgpuDeviceSetUncapturedErrorCallback(t->device, handle_device_error, t);

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









/*
#ifndef _SNES_ROM_
#define _SNES_ROM_

#define snes_rom_schema(X,Y) \
    i_prop    (X,Y, required,   path,               rom_path) \
    i_prop    (X,Y, intern,     cstr,               data) \
    i_prop    (X,Y, intern,     size_t,             size) \
    i_prop    (X,Y, public,     u32,                header_offset) \
    i_prop    (X,Y, public,     u32,                audio_offset) \
    i_prop    (X,Y, public,     map,                banks) \
    i_method  (X,Y, public,     bool,               validate_header) \
    i_method  (X,Y, public,     u32,                find_spc_base) \
    i_method  (X,Y, public,     array,              read_bank, u32) \
    i_override(X,Y, method,     init) \
    i_override(X,Y, method,     destructor)

#ifndef snes_rom_intern
#define snes_rom_intern
#endif
declare_class(snes_rom)

void snes_rom_init(snes_rom rom) {
    path p = rom->rom_path;
    verify(exists(p), "ROM file not found: %o", p);
    
    // Read full ROM file
    FILE* f = fopen(p->chars, "rb");
    verify(f, "Could not open ROM: %o", p);
    fseek(f, 0, SEEK_END);
    rom->size = ftell(f);
    fseek(f, 0, SEEK_SET);
    rom->data = calloc(1, rom->size);
    verify(rom->data, "Memory allocation failed");
    fread(rom->data, 1, rom->size, f);
    fclose(f);

    // Create banks map for memory regions
    rom->banks = map(32);
    
    // Find header and validate
    verify(validate_header(rom), "Invalid SNES ROM header");
    
    // Locate audio program base
    rom->audio_offset = find_spc_base(rom);
    verify(rom->audio_offset, "Could not locate audio program");
}

void snes_rom_destructor(snes_rom rom) {
    if (rom->data) {
        free(rom->data);
        rom->data = null;
    }
}

bool snes_rom_validate_header(snes_rom rom) {
    // Check for SMC header and adjust
    if (rom->size % 1024 == 512) {
        rom->header_offset = 512;
    } else {
        rom->header_offset = 0;
    }

    // Basic SNES header validation 
    u8* header = &rom->data[rom->header_offset + 0x7FB0];
    
    // Maker code should be valid
    if (header[0] < 0x01 || header[0] > 0x33)
        return false;
        
    // ROM size should match file
    u32 rom_size = (1024 << header[1]);
    if (rom_size != (rom->size - rom->header_offset))
        return false;

    return true;
}

array snes_rom_read_bank(snes_rom rom, u32 bank_num) {
    verify(bank_num < (rom->size / 0x8000), "Invalid bank number");
    
    array bank_data = array(0x8000);  // SNES bank size
    u32 offset = rom->header_offset + (bank_num * 0x8000);
    
    // Copy bank data to array
    for (int i = 0; i < 0x8000; i++) {
        push(bank_data, rom->data[offset + i]);
    }
    
    return bank_data;
}

u32 snes_rom_find_spc_base(snes_rom rom) {
    // Simple scan for SPC program signature
    // This would need to be customized per-game or expanded
    // to handle different signature patterns
    u8 signature[] = {0x8F, 0x00, 0xF1}; // Common SPC init
    
    for (u32 i = rom->header_offset; i < rom->size - 3; i++) {
        if (memcmp(&rom->data[i], signature, 3) == 0) {
            return i;
        }
    }
    return 0;
}

#endif



#ifndef _SPC_DATA_
#define _SPC_DATA_

#define spc_data_schema(X,Y) \
   i_prop    (X,Y, required,   snes_rom,           rom) \
   i_prop    (X,Y, intern,     cstr,               ram) \
   i_prop    (X,Y, intern,     map,                regions) \
   i_prop    (X,Y, public,     u16,                dsp_addr) \
   i_prop    (X,Y, public,     u16,                stack_ptr) \
   i_prop    (X,Y, public,     array,              program) \
   i_method  (X,Y, public,     none,               load_region, u16, array) \
   i_method  (X,Y, public,     array,              read_region, u16, u16) \
   i_method  (X,Y, public,     u8,                 read_byte, u16) \
   i_method  (X,Y, public,     u16,                read_word, u16) \
   i_override(X,Y, method,     init) \
   i_override(X,Y, method,     destructor)

#ifndef spc_data_intern
#define spc_data_intern
#endif
declare_class(spc_data)

void spc_data_init(spc_data spc) {
   verify(spc->rom, "ROM required");

   // Allocate 64KB RAM 
   spc->ram = calloc(1, 0x10000);
   verify(spc->ram, "Memory allocation failed");

   // Create memory region map
   spc->regions = map(16);
   
   // Load initial program data from ROM
   u32 audio_base = spc->rom->audio_offset;
   spc->program = array(0x10000);

   // Read program data
   for (int i = 0; i < 0x10000 && (audio_base + i) < spc->rom->size; i++) {
       if (spc->rom->data[audio_base + i] != 0xFF) { // Skip padding
           push(spc->program, spc->rom->data[audio_base + i]);
       }
   }

   // Default hardware registers
   spc->dsp_addr = 0x00F1;  // Standard DSP register base
   spc->stack_ptr = 0x01FF; // Default stack location
}

void spc_data_destructor(spc_data spc) {
   if (spc->ram) {
       free(spc->ram);
       spc->ram = null;
   }
}

none spc_data_load_region(spc_data spc, u16 addr, array data) {
   verify((addr + len(data)) <= 0x10000, "Region exceeds RAM size");
   
   // Copy data to RAM
   for (int i = 0; i < len(data); i++) {
       spc->ram[addr + i] = data->elements[i];
   }

   // Store region info
   region_info info = { addr, len(data) };
   set(spc->regions, (void*)(size_t)addr, info);
}

array spc_data_read_region(spc_data spc, u16 start, u16 length) {
   verify((start + length) <= 0x10000, "Read exceeds RAM size");
   
   array data = array(length);
   for (u16 i = 0; i < length; i++) {
       push(data, spc->ram[start + i]);
   }
   return data;
}

u8 spc_data_read_byte(spc_data spc, u16 addr) {
   verify(addr < 0x10000, "Address out of range");
   return spc->ram[addr];
}

u16 spc_data_read_word(spc_data spc, u16 addr) {
   verify((addr + 1) < 0x10000, "Address out of range");
   return spc->ram[addr] | (spc->ram[addr + 1] << 8);
}

#endif




#ifndef _BRR_SAMPLE_
#define _BRR_SAMPLE_

#define brr_schema(X,Y) \
   i_prop    (X,Y, required,   spc_data,           spc) \
   i_prop    (X,Y, intern,     array,              brr_data) \
   i_prop    (X,Y, public,     array,              pcm_data) \
   i_prop    (X,Y, public,     u16,                loop_point) \
   i_prop    (X,Y, public,     bool,               has_loop) \
   i_prop    (X,Y, intern,     i16,                prev1) \
   i_prop    (X,Y, intern,     i16,                prev2) \
   i_method  (X,Y, public,     array,              decode_block, array) \
   i_method  (X,Y, public,     none,               reset_state) \
   i_override(X,Y, method,     init) \
   i_override(X,Y, method,     destructor)

#ifndef brr_intern
#define brr_intern
#endif
declare_class(brr_sample)

void brr_sample_init(brr_sample brr) {
   verify(brr->spc, "SPC data required");
   brr->brr_data = array(1024);    // Dynamic size
   brr->pcm_data = array(4096);    // Will grow as needed
   brr->prev1 = 0;
   brr->prev2 = 0;
}

void brr_sample_destructor(brr_sample brr) {
   // Arrays handled by A type system
}

// BRR block header bits
enum {
   BRR_END     = 0x01,
   BRR_LOOP    = 0x02,
   BRR_FILTER  = 0x0C,
   BRR_RANGE   = 0xF0
};

array brr_sample_decode_block(brr_sample brr, array block) {
   verify(len(block) == 9, "BRR block must be 9 bytes");
   
   u8 header = block->elements[0];
   array samples = array(16);  // Each block decodes to 16 samples
   
   // Get filter and range values
   int shift = (header & BRR_RANGE) >> 4;
   int filter = (header & BRR_FILTER) >> 2;
   
   // Check for end/loop flags
   brr->has_loop = (header & BRR_LOOP) != 0;
   bool end = (header & BRR_END) != 0;
   
   // Process 8 bytes of compressed data into 16 samples
   for (int i = 0; i < 8; i++) {
       u8 byte = block->elements[i + 1];
       
       // Each byte has two 4-bit samples
       for (int n = 0; n < 2; n++) {
           i16 sample = (byte >> (n * 4)) & 0x0F;
           if (sample > 7) sample -= 16;  // Sign extend
           
           // Shift based on range
           sample = (sample << shift) >> 1;
           
           // Apply IIR filter
           switch (filter) {
               case 0: // Direct
                   break;
                   
               case 1: // 15/16
                   sample += brr->prev1 >> 1;
                   sample += (-brr->prev1) >> 5;
                   break;
                   
               case 2: // 61/32 - 15/16
                   sample += brr->prev1;
                   sample += (-brr->prev1 * 13) >> 7;
                   sample += brr->prev2 >> 1;
                   sample += (-brr->prev2) >> 5;
                   break;
                   
               case 3: // 115/64 - 13/16
                   sample += brr->prev1;
                   sample += (-brr->prev1 * 3) >> 4;
                   sample += brr->prev2 >> 1;
                   sample += (-brr->prev2) >> 3;
                   break;
           }
           
           // Clamp to 16-bit signed range
           if (sample > 32767)  sample = 32767;
           if (sample < -32768) sample = -32768;
           
           // Store sample and update previous values
           push(samples, sample);
           brr->prev2 = brr->prev1;
           brr->prev1 = sample;
       }
   }
   
   return samples;
}

none brr_sample_reset_state(brr_sample brr) {
   brr->prev1 = 0;
   brr->prev2 = 0;
}

#endif




#ifndef _SPC_SEQUENCE_
#define _SPC_SEQUENCE_

#define spc_sequence_schema(X,Y) \
   i_prop    (X,Y, required,   spc_data,           spc) \
   i_prop    (X,Y, public,     u8,                 channel_count) \
   i_prop    (X,Y, public,     map,                patterns) \
   i_prop    (X,Y, public,     array,              timeline) \
   i_prop    (X,Y, intern,     map,                commands) \
   i_prop    (X,Y, public,     u16,                tempo) \
   i_prop    (X,Y, public,     bool,               looping) \
   i_prop    (X,Y, public,     u16,                loop_point) \
   i_method  (X,Y, public,     none,               parse_pattern, u16) \
   i_method  (X,Y, public,     array,              get_events, u32) \
   i_method  (X,Y, public,     none,               read_command, u16) \
   i_override(X,Y, method,     init) \
   i_override(X,Y, method,     destructor)

#ifndef spc_sequence_intern
#define spc_sequence_intern
#endif
declare_class(spc_sequence)

// Common sequence commands
enum {
   CMD_NOTE_ON   = 0x80,
   CMD_NOTE_OFF  = 0x81,
   CMD_VOLUME    = 0x82,
   CMD_PAN       = 0x83,
   CMD_PITCH     = 0x84,
   CMD_TEMPO     = 0x85,
   CMD_LOOP      = 0x86,
   CMD_END       = 0x87
};

void spc_sequence_init(spc_sequence seq) {
   verify(seq->spc, "SPC data required");
   seq->patterns = map(32);       // Pattern storage
   seq->timeline = array(256);    // Event timeline
   seq->commands = map(16);       // Command handlers
   seq->channel_count = 8;        // SNES has 8 channels
   seq->tempo = 120;              // Default tempo
   
   // Register command handlers
   // In real code we'd have function pointers for each command type
}

void spc_sequence_destructor(spc_sequence seq) {
   // Maps and arrays handled by A type system
}

none spc_sequence_parse_pattern(spc_sequence seq, u16 addr) {
   array pattern = array(64);  // Dynamic size pattern
   u16 pos = addr;
   
   while (1) {
       u8 cmd = seq->spc->ram[pos++];
       push(pattern, cmd);
       
       if (cmd == CMD_END) break;
       
       // Parse command data
       switch (cmd & 0xF0) {
           case CMD_NOTE_ON:
               push(pattern, seq->spc->ram[pos++]);  // Note
               push(pattern, seq->spc->ram[pos++]);  // Velocity
               break;
               
           case CMD_NOTE_OFF:
               push(pattern, seq->spc->ram[pos++]);  // Channel
               break;
               
           case CMD_VOLUME:
               push(pattern, seq->spc->ram[pos++]);  // Volume
               break;
               
           case CMD_PAN:
               push(pattern, seq->spc->ram[pos++]);  // Pan
               break;
               
           case CMD_PITCH:
               push(pattern, seq->spc->ram[pos++]);  // Pitch LSB
               push(pattern, seq->spc->ram[pos++]);  // Pitch MSB
               break;
               
           case CMD_TEMPO:
               push(pattern, seq->spc->ram[pos++]);  // New tempo
               break;
               
           case CMD_LOOP:
               seq->looping = true;
               seq->loop_point = seq->spc->ram[pos++];
               break;
       }
   }
   
   // Store the pattern
   set(seq->patterns, (void*)(size_t)addr, pattern);
}

array spc_sequence_get_events(spc_sequence seq, u32 time) {
   array events = array(8);  // Events for this time slice
   
   // Process timeline at given time
   // This would check patterns and active notes
   // Return any events that should trigger
   
   return events;
}

#endif





#ifndef _SPC_INSTRUMENT_
#define _SPC_INSTRUMENT_

#define spc_instrument_schema(X,Y) \
   i_prop    (X,Y, required,   spc_data,           spc) \
   i_prop    (X,Y, required,   brr_sample,         sample) \
   i_prop    (X,Y, public,     u8,                 attack) \
   i_prop    (X,Y, public,     u8,                 decay) \
   i_prop    (X,Y, public,     u8,                 sustain) \
   i_prop    (X,Y, public,     u8,                 release) \
   i_prop    (X,Y, public,     u8,                 base_note) \
   i_prop    (X,Y, public,     i8,                 tune) \
   i_prop    (X,Y, public,     u16,                volume) \
   i_prop    (X,Y, public,     map,                key_map) \
   i_method  (X,Y, public,     none,               load_adsr, u8) \
   i_method  (X,Y, public,     none,               set_tuning, u8, i8) \
   i_method  (X,Y, public,     i16,                get_pitch, u8) \
   i_override(X,Y, method,     init) \
   i_override(X,Y, method,     destructor)

#ifndef spc_instrument_intern
#define spc_instrument_intern
#endif
declare_class(spc_instrument)

void spc_instrument_init(spc_instrument inst) {
   verify(inst->spc, "SPC data required");
   verify(inst->sample, "Sample data required");
   
   // Default ADSR (Attack, Decay, Sustain, Release) values
   inst->attack  = 15;  // Fast attack
   inst->decay   = 7;   // Medium decay
   inst->sustain = 7;   // Medium sustain
   inst->release = 7;   // Medium release
   
   inst->base_note = 60;  // Middle C
   inst->tune = 0;        // No tuning offset
   inst->volume = 100;    // Default volume
   
   // Create key map for multi-sample instruments
   inst->key_map = map(128);  // Full MIDI key range
}

void spc_instrument_destructor(spc_instrument inst) {
   // Maps handled by A type system
}

none spc_instrument_load_adsr(spc_instrument inst, u8 adsr_byte) {
   // SNES ADSR is packed into 2 bytes, here we handle first byte
   inst->attack  = (adsr_byte >> 4) & 0x0F;
   inst->decay   = adsr_byte & 0x07;
   
   // Second byte would set sustain and release
   // Some games used different ADSR formats
}

none spc_instrument_set_tuning(spc_instrument inst, u8 base, i8 tune) {
   inst->base_note = base;
   inst->tune = tune;
}

i16 spc_instrument_get_pitch(spc_instrument inst, u8 note) {
   // Convert MIDI note to SNES frequency value
   i16 offset = note - inst->base_note;
   
   // Apply fine tuning
   offset += inst->tune;
   
   // SNES used a lookup table for pitch values
   // This is simplified - real version would use proper pitch table
   i16 base_pitch = 0x1000;  // Middle C pitch value
   return base_pitch + (offset * 64);
}

#endif





#ifndef _DSP_STATE_
#define _DSP_STATE_

#define dsp_state_schema(X,Y) \
   i_prop    (X,Y, required,   spc_data,           spc) \
   i_prop    (X,Y, intern,     u8[128],            registers) \
   i_prop    (X,Y, public,     u8,                 main_volume_l) \
   i_prop    (X,Y, public,     u8,                 main_volume_r) \
   i_prop    (X,Y, public,     u16,                echo_delay) \
   i_prop    (X,Y, public,     bool,               echo_enabled) \
   i_prop    (X,Y, public,     u8,                 echo_volume_l) \
   i_prop    (X,Y, public,     u8,                 echo_volume_r) \
   i_prop    (X,Y, public,     u8,                 feedback) \
   i_prop    (X,Y, public,     u8,                 noise_clock) \
   i_method  (X,Y, public,     none,               write_reg, u8, u8) \
   i_method  (X,Y, public,     u8,                 read_reg, u8) \
   i_method  (X,Y, public,     none,               set_channel_volume, u8, u8, u8) \
   i_method  (X,Y, public,     none,               set_echo_config, u8, bool) \
   i_override(X,Y, method,     init) \
   i_override(X,Y, method,     destructor)

#ifndef dsp_state_intern
#define dsp_state_intern
#endif
declare_class(dsp_state)

// DSP Register addresses
enum {
   DSP_MVOL_L = 0x0C,
   DSP_MVOL_R = 0x1C,
   DSP_EVOL_L = 0x2C,
   DSP_EVOL_R = 0x3C,
   DSP_KON    = 0x4C,    // Key on
   DSP_KOF    = 0x5C,    // Key off
   DSP_FLG    = 0x6C,    // Flags (noise, echo, mute)
   DSP_EON    = 0x7C,    // Echo on
   DSP_EDL    = 0x7D     // Echo delay
};

void dsp_state_init(dsp_state dsp) {
   verify(dsp->spc, "SPC data required");
   
   // Initialize DSP registers to default values
   memset(dsp->registers, 0, 128);
   
   // Set default volumes
   dsp->main_volume_l = 0x7F;
   dsp->main_volume_r = 0x7F;
   dsp->echo_volume_l = 0;
   dsp->echo_volume_r = 0;
   
   // Default echo settings
   dsp->echo_delay = 0;
   dsp->echo_enabled = false;
   dsp->feedback = 0;
   
   // Default noise settings
   dsp->noise_clock = 0;
   
   // Write initial values to DSP
   write_reg(dsp, DSP_MVOL_L, dsp->main_volume_l);
   write_reg(dsp, DSP_MVOL_R, dsp->main_volume_r);
}

void dsp_state_destructor(dsp_state dsp) {
   // Clean shutdown of DSP
   write_reg(dsp, DSP_FLG, 0x20);  // Mute output
}

none dsp_state_write_reg(dsp_state dsp, u8 addr, u8 val) {
   verify(addr < 128, "Invalid DSP register");
   dsp->registers[addr] = val;
   
   // Update corresponding properties
   switch (addr) {
       case DSP_MVOL_L:
           dsp->main_volume_l = val;
           break;
       case DSP_MVOL_R:
           dsp->main_volume_r = val;
           break;
       case DSP_EDL:
           dsp->echo_delay = val & 0x0F;
           break;
       case DSP_FLG:
           dsp->echo_enabled = !(val & 0x20);
           dsp->noise_clock = val & 0x1F;
           break;
   }
}

u8 dsp_state_read_reg(dsp_state dsp, u8 addr) {
   verify(addr < 128, "Invalid DSP register");
   return dsp->registers[addr];
}

none dsp_state_set_channel_volume(dsp_state dsp, u8 chan, u8 left, u8 right) {
   verify(chan < 8, "Invalid channel number");
   
   // Volume registers are at VOL_L = chan * 0x10
   write_reg(dsp, chan * 0x10, left);
   write_reg(dsp, chan * 0x10 + 1, right);
}

none dsp_state_set_echo_config(dsp_state dsp, u8 delay, bool enabled) {
   dsp->echo_delay = delay & 0x0F;
   dsp->echo_enabled = enabled;
   
   write_reg(dsp, DSP_EDL, delay);
   u8 flg = read_reg(dsp, DSP_FLG);
   if (enabled)
       flg &= ~0x20;
   else
       flg |= 0x20;
   write_reg(dsp, DSP_FLG, flg);
}

#endif



#ifndef _SPC_PLAYER_
#define _SPC_PLAYER_

#define spc_player_schema(X,Y) \
   i_prop    (X,Y, required,   spc_data,           spc) \
   i_prop    (X,Y, required,   dsp_state,          dsp) \
   i_prop    (X,Y, required,   spc_sequence,       sequence) \
   i_prop    (X,Y, public,     map,                instruments) \
   i_prop    (X,Y, public,     array,              active_voices) \
   i_prop    (X,Y, public,     u32,                tick_counter) \
   i_prop    (X,Y, public,     bool,               playing) \
   i_prop    (X,Y, intern,     map,                voice_states) \
   i_method  (X,Y, public,     none,               play) \
   i_method  (X,Y, public,     none,               pause) \
   i_method  (X,Y, public,     none,               stop) \
   i_method  (X,Y, public,     none,               process_tick) \
   i_method  (X,Y, public,     none,               key_on, u8, u8, u8) \
   i_method  (X,Y, public,     none,               key_off, u8) \
   i_override(X,Y, method,     init) \
   i_override(X,Y, method,     destructor)

#ifndef spc_player_intern
#define spc_player_intern
#endif
declare_class(spc_player)

// Voice state tracking
typedef struct {
   u8 channel;
   u8 key;
   u8 velocity;
   spc_instrument instrument;
   bool active;
   u32 start_tick;
} voice_state;

void spc_player_init(spc_player player) {
   verify(player->spc, "SPC data required");
   verify(player->dsp, "DSP state required");
   verify(player->sequence, "Sequence required");
   
   player->instruments = map(32);
   player->active_voices = array(8);  // SNES has 8 voices
   player->voice_states = map(8);
   player->tick_counter = 0;
   player->playing = false;
   
   // Initialize voice states
   for (int i = 0; i < 8; i++) {
       voice_state* vs = calloc(1, sizeof(voice_state));
       vs->channel = i;
       vs->active = false;
       set(player->voice_states, (void*)(size_t)i, vs);
   }
}

void spc_player_destructor(spc_player player) {
   stop(player);
   
   // Free voice states
   pairs(player->voice_states, i) {
       voice_state* vs = i->value;
       free(vs);
   }
}

none spc_player_play(spc_player player) {
   player->playing = true;
}

none spc_player_pause(spc_player player) {
   player->playing = false;
}

none spc_player_stop(spc_player player) {
   player->playing = false;
   player->tick_counter = 0;
   
   // Stop all active voices
   pairs(player->voice_states, i) {
       voice_state* vs = i->value;
       if (vs->active) {
           key_off(player, vs->channel);
       }
   }
}

none spc_player_process_tick(spc_player player) {
   if (!player->playing) return;
   
   // Get events for this tick
   array events = get_events(player->sequence, player->tick_counter);
   
   // Process each event
   each(events, node, event) {
       // Handle event based on type
       u8 type = event->type;
       switch(type) {
           case CMD_NOTE_ON:
               key_on(player, event->channel, event->key, event->velocity);
               break;
               
           case CMD_NOTE_OFF:
               key_off(player, event->channel);
               break;
               
           // Handle other event types...
       }
   }
   
   player->tick_counter++;
}

none spc_player_key_on(spc_player player, u8 chan, u8 key, u8 vel) {
   verify(chan < 8, "Invalid channel");
   
   voice_state* vs = get(player->voice_states, (void*)(size_t)chan);
   if (vs->active) {
       key_off(player, chan);
   }
   
   // Set up the new note
   vs->key = key;
   vs->velocity = vel;
   vs->active = true;
   vs->start_tick = player->tick_counter;
   
   // Get instrument for this channel
   spc_instrument inst = get(player->instruments, (void*)(size_t)chan);
   verify(inst, "No instrument set for channel");
   vs->instrument = inst;
   
   // Calculate pitch
   i16 pitch = get_pitch(inst, key);
   
   // Set DSP registers for this voice
   write_reg(player->dsp, 0x02 + (chan * 0x10), pitch & 0xFF);
   write_reg(player->dsp, 0x03 + (chan * 0x10), pitch >> 8);
   
   // Trigger the note
   write_reg(player->dsp, DSP_KON, 1 << chan);
}

none spc_player_key_off(spc_player player, u8 chan) {
   verify(chan < 8, "Invalid channel");
   
   voice_state* vs = get(player->voice_states, (void*)(size_t)chan);
   if (!vs->active) return;
   
   vs->active = false;
   write_reg(player->dsp, DSP_KOF, 1 << chan);
}

#endif
*/

#endif