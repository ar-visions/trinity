#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <import>
#include <sys/stat.h>

static const int enable_validation = 1;
static PFN_vkCreateDebugUtilsMessengerEXT  _vkCreateDebugUtilsMessengerEXT;
static u32 vk_version = VK_API_VERSION_1_2;

static void handle_glfw_key(
    GLFWwindow *glfw_window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_R && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    }
}

static VkSampleCountFlagBits max_sample_count(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device, &props);
    VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts &
                                props.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

static u32 find_memory_type(trinity t, u32 type_filter, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(t->physical_device, &props);

    for (u32 i = 0; i < props.memoryTypeCount; i++)
        if ((type_filter & (1 << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags)
            return i;

    fault("could not find memory type");
    return UINT32_MAX;
}

static VkBuffer create_buffer(trinity t, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, object data, VkDeviceMemory* mem) {
    VkBuffer buffer;
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult result = vkCreateBuffer(t->device, &buffer_info, null, &buffer);
    verify(result == VK_SUCCESS, "Failed to create buffer");

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(t->device, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(t, mem_requirements.memoryTypeBits, properties),
        .pNext           = &(VkMemoryAllocateFlagsInfo) {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
        }
    };

    VkDeviceMemory buffer_memory;
    result = vkAllocateMemory(t->device, &alloc_info, null, &buffer_memory);
    verify(result == VK_SUCCESS, "Failed to allocate buffer memory");

    vkBindBufferMemory(t->device, buffer, buffer_memory, 0);

    // Copy data if provided
    if (data) {
        void* mapped_memory;
        result = vkMapMemory(t->device, buffer_memory, 0, size, 0, &mapped_memory);
        verify(result == VK_SUCCESS, "Failed to map buffer memory");
        memcpy(mapped_memory, data, size);
        vkUnmapMemory(t->device, buffer_memory);
    }

    // set this to device_memory, so we may release later
    if (mem)
        *mem = buffer_memory;
    else
        set(t->device_memory, buffer, buffer_memory);
    return buffer;
}

VkDeviceMemory device_memory(trinity t, VkBuffer b) {
    pairs (t->device_memory, i) {
        if (i->key == (object)b) return (VkDeviceMemory)i->value;
    }
    fault("device memory not resolved");
    return null;
}

void transition_image_layout(trinity, VkImage, VkImageLayout, VkImageLayout);

static void create_resolve(window w, VkImageViewCreateInfo* image_view_info) {
    trinity t = w->t;
    /// create resolve (for msaa)
    VkImageCreateInfo resolve_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = w->surface_format.format,
        .extent = { w->width, w->height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage resolve_image;
    VkDeviceMemory resolve_memory;
    vkCreateImage(t->device, &resolve_info, null, &resolve_image);
    VkMemoryRequirements resolve_reqs;
    vkGetImageMemoryRequirements(t->device, resolve_image, &resolve_reqs);
    VkMemoryAllocateInfo resolve_alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = resolve_reqs.size,
        .memoryTypeIndex = find_memory_type(t, resolve_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    vkAllocateMemory(t->device, &resolve_alloc, null, &resolve_memory);
    vkBindImageMemory(t->device, resolve_image, resolve_memory, 0);

    // Create resolve image view
    image_view_info->image = resolve_image;
    VkImageView resolve_view;
    vkCreateImageView(t->device, image_view_info, null, &resolve_view);

    w->resolve_image = resolve_image;
    w->resolve_view  = resolve_view;
    w->resolve_memory = resolve_memory;

    transition_image_layout(t, w->resolve_image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

static void create_color_image(window w, VkImageViewCreateInfo* image_view_info) {
    trinity t = w->t;
    /// create color image (for msaa)
    VkImageCreateInfo color_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = w->surface_format.format,
        .extent = { w->width, w->height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = t->msaa_samples,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage color_image;
    VkDeviceMemory color_memory;
    vkCreateImage(t->device, &color_image_info, null, &color_image);
    VkMemoryRequirements color_reqs;
    vkGetImageMemoryRequirements(t->device, color_image, &color_reqs);
    VkMemoryAllocateInfo color_alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = color_reqs.size,
        .memoryTypeIndex = find_memory_type(t, color_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    vkAllocateMemory(t->device, &color_alloc, null, &color_memory);
    vkBindImageMemory(t->device, color_image, color_memory, 0);

    // Create color image view
    image_view_info->image = color_image;
    VkImageView color_view;
    vkCreateImageView(t->device, image_view_info, null, &color_view);

    w->color_image = color_image;
    w->color_view = color_view;
    w->color_memory = color_memory;

    transition_image_layout(t, w->color_image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

static void update_framebuffers(window w) {
    trinity t = w->t;

    // Wait for the device to idle before resizing
    vkDeviceWaitIdle(t->device);

    // Destroy old framebuffers and cleanup old resources
    if (w->framebuffers) {
        for (uint32_t i = 0; i < w->image_count; ++i) {
            vkDestroyFramebuffer(t->device, w->framebuffers[i], null);
        }
        free(w->framebuffers);
    }

    // Clean up old color image if it exists
    if (w->color_image) {
        vkDestroyImageView(t->device, w->color_view, null);
        vkDestroyImage(t->device, w->color_image, null);
        vkFreeMemory(t->device, w->color_memory, null);
    }

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(t->physical_device, w->surface, &capabilities);

    // Update window dimensions to match Vulkan requirements
    if (capabilities.currentExtent.width != UINT32_MAX) {
        // Vulkan specifies an exact extent
        w->width = capabilities.currentExtent.width;
        w->height = capabilities.currentExtent.height;
    } else {
        // Vulkan allows flexible extent; clamp to allowed range
        w->width = clamp(w->width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        w->height = clamp(w->height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    // Recreate the swapchain with new dimensions
    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = w->surface,
        .minImageCount = w->surface_caps.minImageCount,
        .imageFormat = w->surface_format.format,
        .imageColorSpace = w->surface_format.colorSpace,
        .imageExtent = {
            .width = (uint32_t)w->width,
            .height = (uint32_t)w->height,
        },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = w->surface_caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = w->present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = w->swapchain != VK_NULL_HANDLE ? w->swapchain : VK_NULL_HANDLE,
    };

    VkSwapchainKHR old_swapchain = w->swapchain;
    VkResult result = vkCreateSwapchainKHR(t->device, &swapchain_info, null, &w->swapchain);
    verify(result == VK_SUCCESS, "Failed to create swapchain");
    
    if (old_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(t->device, old_swapchain, null);
    }

    if (w->command_buffers) {
        for (int i = 0; i < w->image_count; i++) {
            vkDestroySemaphore(t->device, w->image_available_semaphore[i], null);
            vkDestroySemaphore(t->device, w->render_finished_semaphore[i], null);
            vkDestroyFence(t->device, w->command_fences[i], null);
        }

        vkFreeCommandBuffers(t->device, t->command_pool, w->image_count, w->command_buffers);
        free(w->command_buffers);
        free(w->command_fences);
        free(w->image_available_semaphore);
        free(w->render_finished_semaphore);
    }
    
    // Query new swapchain image count
    vkGetSwapchainImagesKHR(t->device, w->swapchain, &w->image_count, null);

    /// create synchronization semaphores (decoupled from swap-chain)
    w->image_available_semaphore = calloc(w->image_count, sizeof(VkSemaphore));
    w->render_finished_semaphore = calloc(w->image_count, sizeof(VkSemaphore));
    w->command_buffers           = calloc(w->image_count, sizeof(VkCommandBuffer));
    w->command_fences            = calloc(w->image_count, sizeof(VkFence));

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = t->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = w->image_count,
    };

    result = vkAllocateCommandBuffers(t->device, &alloc_info, w->command_buffers);
    verify(result == VK_SUCCESS, "failed to allocate command buffers");

    // Get new swapchain images
    VkImage* swapchain_images = malloc(w->image_count * sizeof(VkImage));
    w->depth_images = malloc(w->image_count * sizeof(VkImage));

    // Basic image view creation info (will be modified for each specific view)
    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = w->surface_format.format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    
    // Create the multisampled color image (this will be attachment [0])
    create_color_image(w, &image_view_info);
    
    // Get the swapchain images
    vkGetSwapchainImagesKHR(t->device, w->swapchain, &w->image_count, swapchain_images);

    // Allocate new framebuffers
    w->framebuffers = malloc(w->image_count * sizeof(VkFramebuffer));
    for (uint32_t i = 0; i < w->image_count; ++i) {
        VkResult result;
        VkSemaphoreCreateInfo semaphore_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        result = vkCreateSemaphore(t->device, &semaphore_info, null, &w->image_available_semaphore[i]);
        verify(result == VK_SUCCESS, "failed to create image available semaphore");
        result = vkCreateSemaphore(t->device, &semaphore_info, null, &w->render_finished_semaphore[i]);
        verify(result == VK_SUCCESS, "failed to create render finished semaphore");

        result = vkCreateFence(t->device, &(VkFenceCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT, // Initially signaled to allow the first use
            }, null, &w->command_fences[i]);
        verify(result == VK_SUCCESS, "failed to create fence");

        // Create swapchain image view (this will be attachment [2] - the resolve target)
        image_view_info.image = swapchain_images[i];
        VkImageView swapchain_view;
        result = vkCreateImageView(t->device, &image_view_info, null, &swapchain_view);
        verify(result == VK_SUCCESS, "Failed to create swapchain image view");

        // Create depth image (this will be attachment [1])
        VkImageCreateInfo depth_image_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_D32_SFLOAT,
            .extent = {
                .width = w->width,
                .height = w->height,
                .depth = 1,
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = t->msaa_samples,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        result = vkCreateImage(t->device, &depth_image_info, null, &w->depth_images[i]);
        verify(result == VK_SUCCESS, "Failed to create depth image");

        // Get memory requirements for the image
        VkMemoryRequirements mem_requirements;
        vkGetImageMemoryRequirements(t->device, w->depth_images[i], &mem_requirements);

        // Allocate memory for the image
        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = mem_requirements.size,
            .memoryTypeIndex = find_memory_type(t, mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };

        VkDeviceMemory depth_image_memory;
        result = vkAllocateMemory(t->device, &alloc_info, null, &depth_image_memory);
        verify(result == VK_SUCCESS, "Failed to allocate depth image memory");

        // Bind the memory to the image
        result = vkBindImageMemory(t->device, w->depth_images[i], depth_image_memory, 0);
        verify(result == VK_SUCCESS, "Failed to bind depth image memory");

        VkImageViewCreateInfo depth_view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = w->depth_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_D32_SFLOAT,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        VkImageView depth_view;
        result = vkCreateImageView(t->device, &depth_view_info, null, &depth_view);
        verify(result == VK_SUCCESS, "Failed to create depth view");

        // Set up the attachments array with the correct order
        VkImageView attachments[3] = {
            w->color_view,  // [0] - Multisampled color attachment
            depth_view,     // [1] - Depth attachment
            swapchain_view  // [2] - Resolve target (swapchain image)
        };

        // Create framebuffer
        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = w->render_pass,
            .attachmentCount = 3,
            .pAttachments = attachments,
            .width = w->width,
            .height = w->height,
            .layers = 1,
        };

        result = vkCreateFramebuffer(t->device, &framebuffer_info, null, &w->framebuffers[i]);
        verify(result == VK_SUCCESS, "Failed to create framebuffer");
    }

    free(swapchain_images);
}

static void handle_glfw_framebuffer_size(GLFWwindow *glfw_window, int width, int height) {
    if (width == 0 && height == 0) return; // Minimized window, no resize needed

    window w = glfwGetWindowUserPointer(glfw_window);
    trinity t = w->t;

    // Update window dimensions
    w->width  = width;
    w->height = height;
    w->extent.width  = width;
    w->extent.height = height;

    // Recreate framebuffers
    update_framebuffers(w);
}

void model_init_pipeline(model m, Node n, Primitive prim, shader s) {
    Model      mdl          = m->id;
    trinity    t            = m->t;
    string     name         = prim->name ? prim->name : string("default");
    i64        indices      = prim->indices;
    Accessor   ai           = get(mdl->accessors, indices);
    i64        mat_id       = prim->material;
    AType      itype        = member_type(ai);
    BufferView iview        = get(mdl->bufferViews, ai->bufferView);
    Buffer     ibuffer      = get(mdl->buffers,     iview->buffer);
    verify(mat_id >= 0 && mat_id < len(mdl->materials), "Invalid material index");
    Material   material     = get(mdl->materials, mat_id);
    int        index        = 0;
    int        vertex_size  = 0;
    int        vertex_count = 0;
    int        member_count = 0;
    int        index_size   = itype->size;
    int        index_count  = iview->byteLength / itype->size;

    char part_id[64];
    sprintf(part_id, "%s", cstring(name));
    print("part_id = %o", name);

    /// initialize vertex members data
    vertex_member_t* members = calloc(16, sizeof(vertex_member_t));
    memset(members, 0, sizeof(vertex_member_t));

    // Process attributes properly with accessor stride and offset
    pairs (prim->attributes, i) {
        string   name     = (string)i->key;
        AType    vtype    = isa(i->value);
        i64      ac_index = *(i64*)i->value;
        Accessor ac       = get(mdl->accessors, ac_index);
        BufferView bv     = get(mdl->bufferViews, ac->bufferView);
        Buffer     b      = get(mdl->buffers, bv->buffer);

        members[member_count].type   = member_type(ac);
        members[member_count].ac     = ac;
        members[member_count].size   = members[member_count].type->size;
        members[member_count].offset = vertex_size;

        print("attribute[%i] = %s", member_count, members[member_count].type->name);
        vertex_size += members[member_count].type->size;
        verify(ac->count, "count not set on accessor");
        verify(!vertex_count || ac->count == vertex_count, "invalid vbo data");
        vertex_count = ac->count;
        member_count++;
    }

    /// allocate for GPU resource
    gpu vbo = gpu(t, t,
        name,         copy_cstr(part_id),
        members,      members,
        member_count, member_count,
        vertex_size,  vertex_size,
        vertex_count, vertex_count,
        index_size,   index_size,
        index_count,  index_count);

    /// fetch data handles
    i8 *vdata = vbo->vertex_data;
    i8 *idata = vbo->index_data;

    /// Write VBO data properly with per-attribute striding
    for (int k = 0; k < member_count; k++) {
        vertex_member_t* mem = &members[k];
        BufferView bv     = get(mdl->bufferViews, mem->ac->bufferView);
        Buffer     b      = get(mdl->buffers, bv->buffer);
        i8*        src    = &((i8*)data(b->data))[bv->byteOffset]; // Correct buffer start
        int        stride = mem->ac->stride;

        for (int j = 0; j < vertex_count; j++) {
            i8* dst  = &vdata[mem->offset + j * vertex_size];
            i8* src0 = &src[j * stride];
            memcpy(dst, src0, mem->size); 
        }
    }

    /// write IBO data (todo: use reference)
    i8* i_src = &((i8*)data(ibuffer->data))[iview->byteOffset];
    memcpy(idata, i_src, iview->byteLength);

    /// Construct pipeline, give uniforms in an order of usage; help user out with world ordering
    World world = null;
    each(m->uniforms, object, u) {
        if (isa(u) == typeid(World)) {
            world = u;
            break;
        }
    }
    bool add_world = false;
    if (!world) {
        world = World();
        add_world = true;
    }
    m->world = world;
    array uniforms = array_of(world, null);
    each(m->uniforms, object, u) {
        if (u != (object)world)
            push(uniforms, u);
    }
    
    mat4f model = mat4f_ident();
    model = mat4f_translate(&model, &n->translation);
    if (n->scale.x != 0 || n->scale.y != 0 || n->scale.z != 0)
        model = mat4f_scale(&model, &n->scale);
    model = mat4f_rotate(&model, &n->rotation);
    
    /// pipeline processes samplers, and creates placeholders
    pipeline pipe = pipeline(
        t,          t,
        w,          m->w,
        s,          s,
        vbo,        vbo,
        model,      model,
        uniforms,   uniforms,
        samplers,   m->samplers);
    push(m->pipelines, pipe);
}

void model_init(model m) {
    Model mdl    = m->id;
    trinity t    = m->t;
    m->pipelines = array();
    if (!m->samplers) m->samplers = array();
    if (!m->uniforms) m->uniforms = array();

    if (m->nodes) {
        each (m->nodes, node, n)
            each (n->parts, part, p)
                model_init_pipeline(m, n->id, p->id, p->s ? p->s : m->shader);
    } else {
        // .. how do we iterate through all Primitives in glTF?
        each(mdl->nodes, Node, n) {
            bool has_mesh = n->mesh > 0;
            if (!has_mesh)
                each (n->fields, string, s) {
                    if (cmp(s, "mesh") == 0) {
                        has_mesh = true;
                        break;
                    }
                }
            if (has_mesh) {
                Mesh mesh = get(mdl->meshes, n->mesh);
                each(mesh->primitives, Primitive, prim)
                    model_init_pipeline(m, n, prim, m->shader);
            }
        }
    }
}

void World_init(World w) {
    mat4f_set_identity(&w->proj);
    mat4f_set_identity(&w->model);
    mat4f_set_identity(&w->view);
}


void model_destructor(model m) {
    /// pipelines should free automatically
}

void transition_image_layout(trinity t, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    // 1. Allocate a command buffer
    VkCommandBufferAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool        = t->command_pool,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(t->device, &allocInfo, &commandBuffer);

    // 2. Begin recording command buffer
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // 3. Define an image memory barrier
    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = oldLayout,
        .newLayout           = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        // Transition from undefined to transfer destination (used before copying image data)
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        // Transition after transfer so it can be used in shaders
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else {
        verify(false, "unsupported layout transition");
    }

    // 4. Submit pipeline barrier
    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0, 0, null, 0, null, 1, &barrier
    );

    vkEndCommandBuffer(commandBuffer);
    vkQueueSubmit(t->queue, 1, &(VkSubmitInfo) {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &commandBuffer
    }, VK_NULL_HANDLE);
    vkQueueWaitIdle(t->queue);
    vkFreeCommandBuffers(t->device, t->command_pool, 1, &commandBuffer);
}


none gpu_sync(gpu a) {
    trinity t = a->t;

    /// uniforms update continuously
    if (a->uniform) {
        AType u_type  = isa(a->uniform);
        if (!a->vk_uniform) {
             a->vk_uniform = buffer(
                t, t, size, u_type->size,
                u_uniform, true, u_dst, true, m_host_visible, true, m_host_coherent, true,
                data, a->uniform);
        } else {
            // map memory, update the uniform buffer, and unmap
            update(a->vk_uniform, a->uniform);
        }
    }

    /// vbo/ibo only need maintenance upon init, less we want to transfer out
    if (a->vertex_size && !a->vk_vertex) {
        VkBufferUsageFlags vertex_usage = 
            (a->compute ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0) |
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        
        VkBufferUsageFlags index_usage = 
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        
        a->vk_vertex = create_buffer(t, a->vertex_size * a->vertex_count, vertex_usage,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, a->vertex_data, null);
        
        if (a->index_size)
            a->vk_index = create_buffer(t, a->index_size * a->index_count, index_usage,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, a->index_data, null);
    }

    if (a->sampler && !a->vk_image) {
        /// check isa(a->sampler) against image, vec3f, vec4f, f32
        image img    = instanceof(a->sampler, image);
        Pixel format = Pixel_none;
        i32 sampler_size = 0;
        u8 fill[256]; // 4 * 4 = 16 * 4 = 64 bytes max, so we use this as staging
            
        if (img) {
            format = img->format;
            sampler_size = byte_count(img);
        } else {
            /// lets create a 4x4 based on a broadcast of data given
            vector_i8    v_i8    = instanceof(a->sampler, vector_i8);     // grayscale i8
            vector_f32   v_f32   = instanceof(a->sampler, vector_f32);    // grayscale f32
            vector_rgb8  v_rgb8  = instanceof(a->sampler, vector_rgb8);   // rgb bytes
            vector_rgbf  v_rgbf  = instanceof(a->sampler, vector_rgbf);   // rgb floats
            vector_rgba8 v_rgba8 = instanceof(a->sampler, vector_rgba8);  // rgba bytes
            vector_rgbaf v_rgbaf = instanceof(a->sampler, vector_rgbaf);  // rgba floats

            i8*    data_i8    = v_i8    ? data(v_i8)    : null;
            f32*   data_f32   = v_f32   ? data(v_f32)   : null;
            rgb8*  data_rgb8  = v_rgb8  ? data(v_rgb8)  : null;
            rgbf*  data_rgbf  = v_rgbf  ? data(v_rgbf)  : null;
            rgba8* data_rgba8 = v_rgba8 ? data(v_rgba8) : null;
            rgbaf* data_rgbaf = v_rgbaf ? data(v_rgbaf) : null;

            /// placeholder samplers, (4x4 is minimum we may create)
            if (data_f32) {
                f32  *f32_fill = (f32*) fill;
                for (int i = 0; i < 4; i++)
                    f32_fill[i] = *data_f32;
                format = Pixel_f32;
                sampler_size = 4 * 4 * sizeof(f32);
            } else if (data_i8) {
                u8  *u8_fill = (u8*) fill;
                for (int i = 0; i < 4*4; i++)
                    u8_fill[i] = *data_i8;
                format = Pixel_u8;
                sampler_size = 4 * 4 * sizeof(u8);
            } else if (data_rgbf) {
                rgbf* v3_fill  = (rgbf*)fill;
                for (int i = 0; i < 4*4; i++) v3_fill[i] = *data_rgbf;
                format = Pixel_rgbf32;
                sampler_size = 4 * 4 * sizeof(rgbf);
            } else if (data_rgb8) {
                rgb8* v3_fill  = (rgb8*)fill;
                for (int i = 0; i < 4*4; i++) v3_fill[i] = *data_rgb8;
                format = Pixel_rgb8;
                sampler_size = 4 * 4 * sizeof(rgb8);
            } else if (data_rgbaf) {
                rgbaf* v4_fill  = (rgbaf*)fill;
                for (int i = 0; i < 4*4; i++) v4_fill[i] = *data_rgbaf;
                format = Pixel_rgbaf32;
                sampler_size = 4 * 4 * sizeof(rgbaf);
            } else if (data_rgba8) {
                rgba8* v4_fill  = (rgba8*)fill;
                for (int i = 0; i < 4*4; i++) v4_fill[i] = *data_rgba8;
                format = Pixel_rgba8;
                sampler_size = 4 * 4 * sizeof(rgba8);
            } else {
                fault("sampler data required");
            }
        }

        VkFormat vk_format =
            format == Pixel_f32     ? VK_FORMAT_R32_SFLOAT : 
            format == Pixel_rgbaf32 ? VK_FORMAT_R32G32B32A32_SFLOAT : 
            format == Pixel_rgbf32  ? VK_FORMAT_R32G32B32_SFLOAT : 
            format == Pixel_rgba8   ? VK_FORMAT_R8G8B8A8_UNORM : 
            format == Pixel_rgb8    ? VK_FORMAT_R8G8B8_UNORM : 
            format == Pixel_u8      ? VK_FORMAT_R8_UNORM : 0;
        verify(format, "incompatible image format: %o", e_str(Pixel, format));

        verify(vkCreateImage(t->device, &(VkImageCreateInfo) {
            .sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format    = vk_format,
            .extent    = { img ? img->width : 2, img ? img->height : 2, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples   = VK_SAMPLE_COUNT_1_BIT,
            .tiling    = VK_IMAGE_TILING_OPTIMAL,
            .usage     = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        }, null, &a->vk_image) == VK_SUCCESS,
            "Failed to create VkImage");

        // Allocate memory and bind
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(t->device, a->vk_image, &memReqs);
        verify(vkAllocateMemory(t->device, &(VkMemoryAllocateInfo) {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = find_memory_type(t, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        }, null, &a->vk_memory) == VK_SUCCESS, 
            "Failed to allocate memory for image");

        vkBindImageMemory(t->device, a->vk_image, a->vk_memory, 0);
        
        VkDeviceMemory stagingMemory;
        VkBuffer stagingBuffer = create_buffer(
            t,
            sampler_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            img ? data(img) : (object)fill, &stagingMemory);

        // Begin Command Buffer Recording
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(t->device, &(VkCommandBufferAllocateInfo) {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandPool        = t->command_pool,
            .commandBufferCount = 1
        }, &commandBuffer);

        vkBeginCommandBuffer(commandBuffer, &(VkCommandBufferBeginInfo) {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        });

        // Transition Image Layout to TRANSFER_DST_OPTIMAL
        VkImageMemoryBarrier barrier = {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = a->vk_image,
            .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
            .srcAccessMask       = 0,
            .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT
        };

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, NULL, 0, NULL, 1, &barrier);

        // Copy Buffer to Image
        VkBufferImageCopy region = {
            .bufferOffset      = 0,
            .bufferRowLength   = 0,  // Tightly packed
            .bufferImageHeight = 0,
            .imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageOffset = {0, 0, 0},
            .imageExtent = { img ? img->width : 2, img ? img->height : 2, 1 }
        };

        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, a->vk_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Transition Image Layout to SHADER_READ_ONLY_OPTIMAL
        barrier.oldLayout    = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, NULL, 0, NULL, 1, &barrier);

        vkEndCommandBuffer(commandBuffer);

        vkQueueSubmit(t->queue, 1, &(VkSubmitInfo) {
            .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers    = &commandBuffer
        }, VK_NULL_HANDLE);
        vkQueueWaitIdle(t->queue);

        // 1. Create Image View for Sampling
        VkImageViewCreateInfo viewInfo = {
            .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image      = a->vk_image,
            .viewType   = VK_IMAGE_VIEW_TYPE_2D,
            .format     = vk_format,
            .subresourceRange = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        };
        verify(vkCreateImageView(t->device, &viewInfo, NULL, &a->vk_image_view) == VK_SUCCESS, 
            "Failed to create VkImageView");

        // 2. Define Sampler Properties
        VkSamplerCreateInfo samplerInfo = {
            .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter               = VK_FILTER_LINEAR,  // Smooth upscaling
            .minFilter               = VK_FILTER_LINEAR,  // Smooth downscaling
            .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .anisotropyEnable        = img ? VK_TRUE : VK_FALSE,
            .maxAnisotropy           = img ? 16 : 1,
            .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            .unnormalizedCoordinates = VK_FALSE,
            .compareEnable           = VK_FALSE,
            .compareOp               = VK_COMPARE_OP_ALWAYS,
            .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        };

        // 3. Create Sampler
        verify(vkCreateSampler(t->device, &samplerInfo, NULL, &a->vk_sampler) == VK_SUCCESS, 
            "Failed to create VkSampler");

        vkFreeCommandBuffers(t->device, t->command_pool, 1, &commandBuffer);
    }

}

none gpu_init(gpu a) {
    if (a->vertex_size) a->vertex_data = A_alloc(typeid(i8), a->vertex_size * a->vertex_count, false);
    if (a->index_size)  a->index_data  = A_alloc(typeid(i8), a->index_size  * a->index_count,  false);
}

none gpu_destructor(gpu a) {
    vkDestroyBuffer(a->t->device, a->vk_vertex, null);
    vkDestroyBuffer(a->t->device, a->vk_index,  null);
    vkFreeMemory   (a->t->device, a->vk_memory, null);
}

define_class(gpu);

VkTransformMatrixKHR convert_matrix_to_transform(float* mat4) {
    // Vulkan expects a 3x4 matrix in row-major order
    // We only take the first 3 rows from the 4x4 matrix
    VkTransformMatrixKHR transform = {
        .matrix = {
            // First row (0-3)
            {mat4[0], mat4[1], mat4[2], mat4[3]},
            // Second row (4-7)
            {mat4[4], mat4[5], mat4[6], mat4[7]},
            // Third row (8-11)
            {mat4[8], mat4[9], mat4[10], mat4[11]}
        }
    };
    return transform;
}

VkIndexType gpu_index_type(gpu a) {
    return a->index_size == 1 ?
        VK_INDEX_TYPE_UINT8_KHR  :
        a->index_size == 2 ?
        VK_INDEX_TYPE_UINT16 :
        VK_INDEX_TYPE_UINT32;
}

void pipeline_bind_resources(pipeline p) {
    trinity t     = p->t;
    int binding_count = 0; // we start at 1, because its occupied by vulkan raytrace data
    int sampler_count = 0;
    int uniform_count = 0;
    VkDescriptorSetLayoutBinding bindings         [16];
    VkDescriptorBufferInfo       buffer_infos     [16];
    VkDescriptorImageInfo        image_infos      [16];
    VkWriteDescriptorSet         descriptor_writes[16];

    p->resources = array(alloc, 32);
    if (p->vbo) {
        sync(p->vbo);
        push(p->resources, p->vbo);
    }
    
    VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;

    // populate bind layout for uniforms
    each(p->uniforms, object, u) {
        gpu    res   = gpu(t, t, name, "uniform", uniform, u);
        sync(res);
        push(p->resources, res);
        AType  type  = isa(res->uniform); // Get type info
        i32    size  = type->size;  // Get struct size
        
        bindings[binding_count] = (VkDescriptorSetLayoutBinding) {
            .binding         = binding_count,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = stage_flags
        };
        binding_count++;
        buffer_infos[uniform_count]  = (VkDescriptorBufferInfo) {
            .buffer = res->vk_uniform->vk_buffer,
            .offset = 0,
            .range = size
        };
        uniform_count++;
    }

    // add sampler2Ds from Surface enum members (same name, and we must get the exact count of sampler2D members (1 for gray, 3 for rgb, 4 for rgba)
    // ok, so these are provided as images, but these only serve to override whats there.
    Surface_f* surface_t = &Surface_type;
    for (int i = 0; i < surface_t->member_count; i++) {
        type_member_t* mem = &surface_t->members[i];
        if (mem->member_type != A_MEMBER_ENUMV) continue;
        AType meta_cl = mem->args.meta_0;
        verify(meta_cl, "meta data not set on Surface/extension");
        int   surface_value = i;
        if (!meta_cl || surface_value == 0) continue;
        gpu res = null;

        /// check if user provides an image
        each (p->samplers, image, img) {
            if ((int)img->surface == surface_value) {
                res = gpu(t, t, name, cstring(e_str(Surface, surface_value)), sampler, img);
                break;
            }
        }

        /// create resource fragment based on the texture type
        if (!res) {
            rgbaf f_normal = rgbaf(0.5f, 0.5f, 1.0f, 1.0f);
            rgbaf f_zero   = rgbaf(0.0f, 0.0f, 0.0f, 0.0f);
            rgbaf f_one    = rgbaf(1.0f, 1.0f, 1.0f, 1.0f);
            shape single = shape_new(1, 0);
            switch (surface_value) {
                case Surface_normal:
                    res = gpu(t, t, name, "normal", sampler,
                        vector_rgbaf_new(single, f_normal)); // Default normal map
                    break;
                case Surface_metal:
                    res = gpu(t, t, name, "metal", sampler,
                        vector_f32_new(single, 0.0f)); // Default: Non-metallic
                    break;
                case Surface_rough:
                    res = gpu(t, t, name, "rough", sampler,
                        vector_f32_new(single, 1.0f)); // Default: Fully rough
                    break;
                case Surface_emission:
                    res = gpu(t, t, name, "emission", sampler,
                        vector_rgbaf_new(single, f_zero)); // Default: No emission
                    break;
                case Surface_height:
                    res = gpu(t, t, name, "height", sampler,
                        vector_f32_new(single, 0.5f)); // Midpoint height (todo: should be 0 but look at this in pbr later)
                    break;
                case Surface_ao:
                    res = gpu(t, t, name, "ao", sampler,
                        vector_f32_new(single, 1.0f)); // Full ambient occlusion by default
                    break;
                case Surface_color:
                    res = gpu(t, t, name, "color", sampler,
                        vector_rgbaf_new(single, f_one)); // White base color
                    break;
                case Surface_environment:
                    res = gpu(t, t, name, "environment", sampler,
                        vector_rgbaf_new(single, f_one)); // White base color
                    break;
                default:
                    verify(0, "Unhandled Surface enum value");
                    break;
            }
        }

        /// add gpu resource; this is what will be updated when required
        sync(res);
        push(p->resources, res);

        bindings[binding_count] = (VkDescriptorSetLayoutBinding) {
            .binding = binding_count,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = stage_flags
        };
        binding_count++;

        image_infos[sampler_count] = (VkDescriptorImageInfo) {
            .sampler     = res->vk_sampler,
            .imageView   = res->vk_image_view, 
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        sampler_count++;
    }

    vkCreateDescriptorSetLayout(p->t->device, &(VkDescriptorSetLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = binding_count,
        .pBindings = bindings
    }, null, &p->descriptor_layout);

    vkCreateDescriptorPool(p->t->device, &(VkDescriptorPoolCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 2,
        .pPoolSizes = &(VkDescriptorPoolSize[2]) {
            { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         .descriptorCount = uniform_count },
            { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = sampler_count }
        },
        .maxSets = 1
    }, null, &p->descriptor_pool);

    vkAllocateDescriptorSets(p->t->device, &(VkDescriptorSetAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = p->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &p->descriptor_layout
    }, &p->descriptor_set);
    
    // Update descriptor set with uniform buffers
    for (int i = 0; i < uniform_count; i++) {
        descriptor_writes[i] = (VkWriteDescriptorSet) {
            .sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet             = p->descriptor_set,
            .dstBinding         = i,
            .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount    = 1,
            .pBufferInfo        = &buffer_infos[i]
        };
    }
    for (int i = 0; i < sampler_count; i++) {
        descriptor_writes[uniform_count + i] = (VkWriteDescriptorSet) {
            .sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet             = p->descriptor_set,
            .dstBinding         = uniform_count + i,
            .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount    = 1,
            .pImageInfo         = &image_infos[i]
        };
    }
    vkUpdateDescriptorSets(p->t->device, uniform_count + sampler_count, descriptor_writes, 0, null);
}

void pipeline_init(pipeline p) {
    gpu vbo = p->vbo, compute = p->memory;
    p->vbo    = vbo;
    p->memory = compute;

    /// uniforms need to be bound, as well as samplers; lets compile the shader and verify
    /// the #include must be processed after the file is copied in Makefile
    /// however we might need to compile the shader at build; most targets have no access to glslang
    /// to make this simple, lets support #include in our actual source here
    
    //if (p->memory) sync(p->memory);
    verify(vbo, "no vbo or memory provided to form a compute or graphical pipeline");
    i32     vertex_size  = vbo->vertex_size;
    i32     vertex_count = vbo->vertex_count;
    i32     index_size   = vbo->index_size;
    i32     index_count  = vbo->index_count;
    trinity t            = p->t;
    window  w            = p->w;

    bind_resources(p);
    
    // Pipeline Layout (Bindings and Layouts)
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &p->descriptor_layout,
        .pushConstantRangeCount = 0,
    };

    VkResult         result = vkCreatePipelineLayout(
        t->device, &pipeline_layout_info, null, &p->layout);
    verify(result == VK_SUCCESS, "Failed to create pipeline layout");

    if (vbo) {
        // Graphics Pipeline
        VkVertexInputBindingDescription binding_description = {
            .binding = 0,
            .stride = vbo->vertex_size,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };

        int attr_count = 0;
        VkVertexInputAttributeDescription* attributes = calloc(vbo->member_count, sizeof(VkVertexInputAttributeDescription));
        for (int i = 0; i < vbo->member_count; i++) {
            vertex_member_t* mem = &vbo->members[i];
            attributes[attr_count].binding = 0;
            attributes[attr_count].location = attr_count;
            attributes[attr_count].format = 
                (mem->type == typeid(vec2f)) ? VK_FORMAT_R32G32_SFLOAT       :
                (mem->type == typeid(vec3f)) ? VK_FORMAT_R32G32B32_SFLOAT    :
                (mem->type == typeid(vec4f)) ? VK_FORMAT_R32G32B32A32_SFLOAT : 
                (mem->type == typeid( f32 )) ? VK_FORMAT_R32_SFLOAT          :
                (mem->type == typeid( i8  )) ? VK_FORMAT_R8_SINT             :
                (mem->type == typeid( u8  )) ? VK_FORMAT_R8_UINT             :
                (mem->type == typeid( i16 )) ? VK_FORMAT_R16_SINT            :
                (mem->type == typeid( u16 )) ? VK_FORMAT_R16_UINT            :
              //(mem->type == typeid( i32 )) ? VK_FORMAT_R32_SINT            : -- glTF does not support
                (mem->type == typeid( u32 )) ? VK_FORMAT_R32_UINT            :
                                               VK_FORMAT_UNDEFINED;
            if (attributes[attr_count].format == VK_FORMAT_UNDEFINED) {
                int test2 = 2;
                test2 += 2;
            }
            attributes[attr_count].offset = mem->offset;
            attr_count++;
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &binding_description,
            .vertexAttributeDescriptionCount = attr_count,
            .pVertexAttributeDescriptions = attributes,
        };
    
        // Add input assembly state for line strip topology
        VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST // specify line list topology (we should wrap our buffer in something that dictates its usage and topology)
        };

        /// rt_support was here, but it doubles the amount of code nee
        VkPipelineShaderStageCreateInfo shader_stages[4];
        int n_stages = 2;
        
        // Regular graphics pipeline setup (vertex & fragment)
        VkPipelineShaderStageCreateInfo vert_shader_stage_info = {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = p->s->vk_vert,
            .pName  = "main"
        };

        VkPipelineShaderStageCreateInfo frag_shader_stage_info = {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = p->s->vk_frag,
            .pName  = "main"
        };

        shader_stages[0] = vert_shader_stage_info;
        shader_stages[1] = frag_shader_stage_info;
        
        // Add viewport and rasterization states
        VkPipelineViewportStateCreateInfo viewport_state = {
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount   = 1,
            .pViewports      = null, // Set dynamically
            .scissorCount    = 1,
            .pScissors       = null, // Set dynamically
        };

        VkPipelineRasterizationStateCreateInfo rasterization_state = {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            .lineWidth               = 1.0f,
            .cullMode                = VK_CULL_MODE_BACK_BIT,
            .frontFace               = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable         = VK_FALSE,
        };

        VkPipelineMultisampleStateCreateInfo multisample_state = {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples  = t->msaa_samples,
            .sampleShadingEnable   = VK_TRUE,
        };

        VkPipelineColorBlendAttachmentState color_blend_attachment = {
            .blendEnable           = VK_FALSE,
            .colorWriteMask        = VK_COLOR_COMPONENT_R_BIT |
                                        VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT |
                                        VK_COLOR_COMPONENT_A_BIT,
        };

        VkPipelineColorBlendStateCreateInfo color_blend_state = {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable         = VK_FALSE,
            .attachmentCount       = 1,
            .pAttachments          = &color_blend_attachment,
        };

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            // Add more dynamic states if needed (e.g., VK_DYNAMIC_STATE_LINE_WIDTH)
        };

        VkPipelineDynamicStateCreateInfo dynamic_state = {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount   = sizeof(dynamic_states) / sizeof(dynamic_states[0]),
            .pDynamicStates      = dynamic_states,
        };

        // Create the graphics pipeline
        VkGraphicsPipelineCreateInfo pipeline_info = {
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount          = n_stages,
            .pStages             = shader_stages,
            .pDepthStencilState  = &(VkPipelineDepthStencilStateCreateInfo) {
                .sType                  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable        = VK_TRUE,
                .depthWriteEnable       = VK_TRUE,
                .depthCompareOp         = VK_COMPARE_OP_LESS,
                .depthBoundsTestEnable  = VK_FALSE,
                .stencilTestEnable      = VK_FALSE
            },
            .pVertexInputState   = &vertex_input_info,
            .pInputAssemblyState = &input_assembly_info,
            .pViewportState      = &viewport_state,
            .pRasterizationState = &rasterization_state,
            .pMultisampleState   = &multisample_state,
            .pColorBlendState    = &color_blend_state,
            .pDynamicState       = &dynamic_state,
            .layout              = p->layout,
            .renderPass          = w->render_pass,
            .subpass             = 0,
            .basePipelineHandle  = VK_NULL_HANDLE,
        };

        result = vkCreateGraphicsPipelines(t->device, VK_NULL_HANDLE, 1, &pipeline_info, null, &p->vk_render);
        verify(result == VK_SUCCESS, "pipeline creation fail");
    
        free(attributes);
    } else if (p->s && p->s->vk_comp) {
        VkComputePipelineCreateInfo compute_pipeline_info = {
            .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .layout = p->layout,
            .stage  = {
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = p->s->vk_comp,
                .pName  = "main",
            },
        };
        result = vkCreateComputePipelines(
            t->device, VK_NULL_HANDLE, 1, &compute_pipeline_info, null, &p->vk_compute);
        verify(result == VK_SUCCESS, "Failed to create compute pipeline");
    }
}


void pipeline_destructor(pipeline p) {
    trinity t = p->t;
    vkDestroyPipelineLayout(t->device, p->layout, null);
    if (p->vk_render)     vkDestroyPipeline(t->device, p->vk_render,     null);
    if (p->vk_compute)    vkDestroyPipeline(t->device, p->vk_compute,    null);
}

void Buffer_init(Buffer b) {
    vector    data = vector(b->uri);
    b->data = data;
}

void pipeline_render(pipeline p, handle f) {
    VkCommandBuffer frame = f;

    /// for each resource of uniform, call sync
    each(p->resources, gpu, res) {
        if (res->uniform)
            sync(res);
    }
    if (p->vk_compute) {
        vkCmdBindPipeline(frame, VK_PIPELINE_BIND_POINT_COMPUTE, p->vk_compute);
        vkCmdBindDescriptorSets(frame, VK_PIPELINE_BIND_POINT_COMPUTE, p->layout, 0, 1, &p->bind, 0, null);
        vkCmdDispatch(frame, p->memory->vertex_count, 1, 1);
    }
    if (p->vk_render) {
        vkCmdBindPipeline(frame, VK_PIPELINE_BIND_POINT_GRAPHICS, p->vk_render);
        VkDeviceSize offsets[] = { 0 };

        vkCmdBindDescriptorSets(frame, VK_PIPELINE_BIND_POINT_GRAPHICS, p->layout, 0, 1, &p->descriptor_set, 0, null);
        vkCmdBindVertexBuffers(frame, 0, 1, &p->vbo->vk_vertex, offsets);

        if (p->vbo->vk_index) {
            vkCmdBindIndexBuffer(frame, p->vbo->vk_index, 0,
                gpu_index_type(p->vbo));
            vkCmdDrawIndexed(frame, p->vbo->index_count, 1, 0, 0, 0);
        } else
            vkCmdDraw(frame, p->vbo->vertex_count, 1, 0, 0);
    }
}

void window_push(window w, model m) {
    array a = w->models;
    push(a, (object)m);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
trinity_log(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData) {
    print("[vulkan] %s", callbackData->pMessage);
    return VK_FALSE;
}

static void set_queue_index(trinity t, window w) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(t->physical_device, &queue_family_count, null);
    verify(queue_family_count > 0, "failed to find queue families");

    VkQueueFamilyProperties* queue_families = calloc(queue_family_count, sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(t->physical_device, &queue_family_count, queue_families);
    bool found = false;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(t->physical_device, i, w->surface, &present_support);
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && present_support) {
            t->queue_family_index = i;  // Select queue that supports both graphics and presentation
            found = true;
            break;
        }
    }
    free(queue_families);
    verify(found, "failed to find a suitable graphics and presentation queue family");
}

void window_init(window w) {
    trinity t = w->t;

    w->models = array();
    // Initialize GLFW window with Vulkan compatibility
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    w->window = glfwCreateWindow(w->width, w->height,
        w->title ? cstring(w->title) : "trinity", null, null);
    verify(w->window, "Failed to create GLFW window");

    glfwSetWindowUserPointer(w->window, (void *)w);
    glfwSetKeyCallback(w->window, handle_glfw_key);
    glfwSetFramebufferSizeCallback(w->window, handle_glfw_framebuffer_size);

    // Create Vulkan surface
    VkResult result = glfwCreateWindowSurface(t->instance, w->window, null, &w->surface);
    verify(result == VK_SUCCESS, "Failed to create Vulkan surface");

    /// this is done once for device, not once per window; however we must have a surface for it
    if (t->queue_family_index == -1) {
        set_queue_index(t, w);
        vkGetDeviceQueue(t->device, t->queue_family_index, 0, &t->queue);
        verify(t->queue, "Failed to retrieve graphics queue");

        result = _vkCreateDebugUtilsMessengerEXT(t->instance, &(VkDebugUtilsMessengerCreateInfoEXT) {
                .sType              = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType        = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback    = trinity_log,
                .pUserData          = t
            }, null, &t->debug);
        verify(result == VK_SUCCESS, "debug messenger");

        /// buffer management for pipeline, without management (it does not drop/hold this external data; not A-type)
        t->device_memory = map(unmanaged, true);
      //t->buffers       = map(unmanaged, true); -- decentralized buffer management
        t->skia          = skia_init_vk(t->instance, t->physical_device, t->device, t->queue, t->queue_family_index, vk_version);
    }
    
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = t->queue_family_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    result = vkCreateCommandPool(t->device, &pool_info, null, &t->command_pool);
    verify(result == VK_SUCCESS, "failed to create command pool");


    // Query surface capabilities
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(t->physical_device, w->surface, &w->surface_caps);
    verify(result == VK_SUCCESS, "Failed to query surface capabilities");

    // Query supported surface formats
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(t->physical_device, w->surface, &format_count, null);
    verify(format_count > 0, "No surface formats found");

    VkSurfaceFormatKHR* formats = malloc(format_count * sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(t->physical_device, w->surface, &format_count, formats);

    // Choose a surface format
    w->surface_format = formats[0];  // Default to the first format
    for (uint32_t i = 0; i < format_count; ++i) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            w->surface_format = formats[i];
            break;
        }
    }
    free(formats);

    // Query supported present modes
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(t->physical_device, w->surface, &present_mode_count, null);
    verify(present_mode_count > 0, "No present modes found");

    VkPresentModeKHR* present_modes = malloc(present_mode_count * sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(t->physical_device, w->surface, &present_mode_count, present_modes);

    // Choose a present mode
    w->present_mode = VK_PRESENT_MODE_FIFO_KHR;  // Default to FIFO (V-Sync)
    for (uint32_t i = 0; i < present_mode_count; ++i) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            w->present_mode = VK_PRESENT_MODE_MAILBOX_KHR;  // Prefer Mailbox (low latency)
            break;
        }
    }
    free(present_modes);

    // Configure swapchain extent
    int width, height;
    glfwGetWindowSize(w->window, &width, &height);
    w->extent.width = (uint32_t)width;
    w->extent.height = (uint32_t)height;

    if (w->surface_caps.currentExtent.width != UINT32_MAX) {
        w->extent = w->surface_caps.currentExtent;  // Use the surface-defined extent if available
    } else {
        // Clamp extent to allowed dimensions
        w->extent.width  = max(w->surface_caps.minImageExtent.width,
                           min(w->surface_caps.maxImageExtent.width, w->extent.width));
        w->extent.height = max(w->surface_caps.minImageExtent.height,
                           min(w->surface_caps.maxImageExtent.height, w->extent.height));
    }

    // create render pass
    VkAttachmentDescription attachments[3] = {
        {
            // [0] - Multisampled color attachment
            .format         = w->surface_format.format,
            .samples        = t->msaa_samples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE, // We don't need to store this, just resolve it
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        }, 
        {
            // [1] - Depth attachment
            .format         = VK_FORMAT_D32_SFLOAT,
            .samples        = t->msaa_samples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        }, 
        {
            // [2] - Resolve attachment (swapchain image)
            .format         = w->surface_format.format,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        }
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depth_attachment_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference resolve_attachment_ref = {
        .attachment = 2,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &color_attachment_ref,
        .pDepthStencilAttachment = &depth_attachment_ref,
        .pResolveAttachments     = &resolve_attachment_ref
    };

    // Add subpass dependencies to ensure proper image layout transitions
    VkSubpassDependency dependencies[2] = {
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        },
        {
            .srcSubpass      = 0,
            .dstSubpass      = VK_SUBPASS_EXTERNAL,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        }
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount        = 3,
        .pAttachments           = attachments,
        .subpassCount           = 1,
        .pSubpasses             = &subpass,
        .dependencyCount        = 2,
        .pDependencies          = dependencies
    };

    result = vkCreateRenderPass(t->device, &render_pass_info, null, &w->render_pass);
    verify(result == VK_SUCCESS, "failed to create render pass");
    update_framebuffers(w);  // Call the framebuffer update directly
}

int window_loop(window w, ARef callback, ARef arg) {
    trinity t = w->t;
    int semaphore_frame = 0;
    int last_fence = -1;
    void(*user_fn)(pipeline, ARef) = callback;

    while (!glfwWindowShouldClose(w->window)) {
        glfwPollEvents();
        
        uint32_t index;
        VkResult result;

        /// below is where we are using a fence
        result = vkAcquireNextImageKHR(
            t->device, w->swapchain, UINT64_MAX,
            w->image_available_semaphore[semaphore_frame], VK_NULL_HANDLE, &index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            handle_glfw_framebuffer_size(w->window, w->width, w->height);
            continue;
        }
        
        verify(result == VK_SUCCESS, "failed to acquire swapchain image");

        vkResetFences  (t->device, 1, &w->command_fences[index]);
        last_fence = index;
        // Begin command buffer recording
        VkCommandBuffer frame = w->command_buffers[index];
        vkResetCommandBuffer(frame, 0);
        vkBeginCommandBuffer(frame, &(VkCommandBufferBeginInfo) {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        });
        vkCmdSetViewport(frame, 0, 1, &(VkViewport) {
            .x                  = 0.0f,
            .y                  = 0.0f,
            .width              = (float)w->extent.width,
            .height             = (float)w->extent.height,
            .minDepth           = 0.0f,
            .maxDepth           = 1.0f
        });
        vkCmdSetScissor(frame, 0, 1, &(VkRect2D) {
            .offset             = {0, 0},
            .extent             = {w->extent.width, w->extent.height}
        });
        vkCmdBeginRenderPass(frame, &(VkRenderPassBeginInfo) {
            .sType              = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass         = w->render_pass,  // Pre-configured render pass
            .framebuffer        = w->framebuffers[index],  // Framebuffer for this image
            .renderArea         = {
                .offset         = { 0, 0 },
                .extent         = w->extent,  // Swapchain image extent
            },
            .clearValueCount    = 3,
            .pClearValues       = &(VkClearValue[3]) {
                { .color        = { .float32 = { 0.2f, 0.0f, 0.5f, 1.0f }} },
                { .depthStencil = { .depth   =   1.0f,
                                    .stencil =   0 } },
                { .color        = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} } }
            }
        }, VK_SUBPASS_CONTENTS_INLINE);

        each(w->models, model, m) {
            
            each(m->pipelines, pipeline, p) {
                // update World uniform with our model default (from glTF)
                each(p->resources, gpu, res)
                    if (res->uniform && isa(res->uniform) == typeid(World)) {
                        ((World)res->uniform)->model = p->model;
                    }
                if (user_fn)
                    user_fn(p, arg);
                // user may indeed set uniforms different depending on the pipeline; this is quite natural and not always redundant
                render(p, frame);
            }
        }
        vkCmdEndRenderPass(frame);   // End render pass
        vkEndCommandBuffer(frame);   // End command buffer recording

        // Submit command buffer to the queue
        VkPipelineStageFlags s_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &frame,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &w->image_available_semaphore[semaphore_frame],
            .pWaitDstStageMask = &s_flags,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &w->render_finished_semaphore[semaphore_frame]
        };

        vkQueueSubmit(t->queue, 1, &submitInfo, w->command_fences[index]);

        // Present the swapchain image
        VkPresentInfoKHR presentInfo = {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .swapchainCount     = 1,
            .pSwapchains        = &w->swapchain,
            .pImageIndices      = &index,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &w->render_finished_semaphore[semaphore_frame]
        };

        result = vkQueuePresentKHR(t->queue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            semaphore_frame = (semaphore_frame + 1) % w->image_count;
            continue;
        }

        verify(result == VK_SUCCESS, "present");
        result = vkWaitForFences(t->device, 1, &w->command_fences[index], VK_TRUE, UINT64_MAX);
        verify(result == VK_SUCCESS, "fence wait failed: last_fence=%d", last_fence);

        semaphore_frame = (semaphore_frame + 1) % w->image_count;
        verify(result == VK_SUCCESS, "failed to present swapchain image");
    }
    return 0;
}

void window_destructor(window w) {
    trinity t = w->t;
    
    // Clean up synchronization objects
    for (int i = 0; i < w->image_count; i++) {
        vkDestroySemaphore(t->device, w->image_available_semaphore[i], null);
        vkDestroySemaphore(t->device, w->render_finished_semaphore[i], null);
        vkDestroyFence    (t->device, w->command_fences[i], null);
    }
    
    // Clean up framebuffers
    if (w->framebuffers) {
        for (uint32_t i = 0; i < w->image_count; ++i) {
            vkDestroyFramebuffer(t->device, w->framebuffers[i], null);
        }
        free(w->framebuffers);
    }
    
    // Clean up MSAA color image
    if (w->color_image) {
        vkDestroyImageView(t->device, w->color_view, null);
        vkDestroyImage(t->device, w->color_image, null);
        vkFreeMemory(t->device, w->color_memory, null);
    }
    
    // Clean up depth images
    if (w->depth_images) {
        for (uint32_t i = 0; i < w->image_count; ++i) {
            if (w->depth_images[i]) {
                // Note: You'll need to store depth view and memory per image
                vkDestroyImage(t->device, w->depth_images[i], null);
                // Free depth memory
            }
        }
        free(w->depth_images);
    }
    
    free(w->command_buffers);
    free(w->command_fences);
    free(w->image_available_semaphore);
    free(w->render_finished_semaphore);
    
    vkDestroyRenderPass(t->device, w->render_pass, null);
    vkDestroySwapchainKHR(t->device, w->swapchain, null);
    vkDestroySurfaceKHR(t->instance, w->surface, null);
    glfwDestroyWindow(w->window);
}

#define BUFFER_SIZE         4096
#define MAX_IMPORT_NAME     256
#define INCLUDE_PREFIX      "#include <"
#define INCLUDE_SUFFIX      ">"

void replace_includes(const char *src, const char *dst) {
    FILE *src_file = fopen(src, "r");
    verify(src_file != NULL, "source not found");

    FILE *dst_file = fopen(dst, "w");
    verify(dst_file != NULL, "destination not found");

    char buffer[BUFFER_SIZE];
    while (fgets(buffer, sizeof(buffer), src_file)) {
        char *include_start = strstr(buffer, INCLUDE_PREFIX);
        if (include_start) {
            char *include_end = strstr(include_start, INCLUDE_SUFFIX);
            if (include_end) {
                // Extract filename inside #include <>
                char import_name[MAX_IMPORT_NAME];
                size_t name_length = include_end - (include_start + strlen(INCLUDE_PREFIX));
                if (name_length >= MAX_IMPORT_NAME) {
                    fprintf(stderr, "Error: Import name too long\n");
                    exit(2);
                }

                strncpy(import_name, include_start + strlen(INCLUDE_PREFIX), name_length);
                import_name[name_length] = '\0';

                string import = form(string, "shaders/%s", import_name);

                // Open the import file
                FILE *import_file = fopen(import->chars, "r");
                if (!import_file) {
                    fprintf(stderr, "Error: Failed to open import file: %s\n", import_name);
                    exit(2);
                }

                // Write import file contents to destination
                while (fgets(buffer, sizeof(buffer), import_file)) {
                    fputs(buffer, dst_file);
                }
                fclose(import_file);
                continue;
            }
        }
        // Write original content if no include directive found
        fputs(buffer, dst_file);
    }

    fclose(src_file);
    fclose(dst_file);

    printf("dst file: %s\n", src);
    printf("dst file: %s\n", dst);

    //exit(2);
}

void shader_init(shader s) {
    // generate .spv for shader resources
    trinity t = s->t;
    string spv_file;
    bool found = false;
    
    s->frag = form(string, "shaders/%o.frag", s->name);
    s->vert = form(string, "shaders/%o.vert", s->name);
    s->comp = form(string, "shaders/%o.comp", s->name);

    char cwd[255]; // PATH_MAX is system-defined maximum path length

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
    } else {
        perror("getcwd() error");
    }
    print("cwd = %s", cwd);
    string names[3] = {
        s->frag, s->vert,  s->comp
    };
    VkShaderModule* values[3] = {
        &s->vk_frag, &s->vk_vert,  &s->vk_comp
    };
    for (int i = 0; i < 3; i++) {
        string name = names[i];
        if (!name) continue;

        path input = form(path, "%o", name);
        if (!name || !file_exists("%o", input)) continue;
        spv_file = form(string, "%o.spv", name);
        found = true;

        // Check if the .spv file already exists and is up to date
        struct stat source_stat, spv_stat;
        int src_exists = stat(input->chars,    &source_stat) == 0;
        int spv_exists = stat(spv_file->chars, &spv_stat)    == 0;
     
        /// if no spv, or our source is newer than the binary (we have changed it!)
        if (!spv_exists || source_stat.st_mtime > spv_stat.st_mtime) {
            // mktemp, and start reading input
            string ext = ext(input);
            path tmp = path_temp(ext->chars); // src = path of temp file
            replace_includes(input->chars, tmp->chars);

            // parse #include <file> and replace with contents of file
            // write(tmp, string)

            // Compile GLSL to SPIR-V using glslangValidator
            string command = form(string, "glslangValidator -V100 --target-env vulkan1.2 %o -o %o", tmp, spv_file);
            print("compiling shader: %o", command);
            int result = system(command->chars);
            if (result != 0) {
                print("shader compilation failed for: %o", name);
                exit(1);
            }
        } else {
            print("using cached SPIR-V file: %o", spv_file);
        }

        // Load SPIR-V binary
        FILE* f = fopen(spv_file->chars, "rb");
        if (!f) {
            print("failed to open SPIR-V file: %o", spv_file);
            exit(1);
        }

        fseek(f, 0, SEEK_END);
        long length = ftell(f);
        rewind(f);

        uint32_t* spirv = malloc(length);
        if (!spirv) {
            fclose(f);
            print("Failed to allocate memory for SPIR-V");
            exit(1);
        }

        fread(spirv, 1, length, f);
        fclose(f);

        // Create Vulkan shader module
        VkShaderModuleCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = (size_t)length,
            .pCode = spirv,
        };

        VkShaderModule module;
        VkResult vkResult = vkCreateShaderModule(t->device, &createInfo, null, &module);
        if (vkResult != VK_SUCCESS) {
            print("Failed to create Vulkan shader module");
            free(spirv);
            exit(1);
        }

        *(values[i]) = module;
        free(spirv);
        print("shader module created: %o", spv_file);
    }
    verify(found, "shader not found: %o", s->name);
}




void shader_destructor(shader s) {
    trinity t = s->t;
    vkDestroyShaderModule(t->device, s->vk_vert, null);
    vkDestroyShaderModule(t->device, s->vk_frag, null);
    vkDestroyShaderModule(t->device, s->vk_comp, null);
}


void get_required_extensions(const char*** extensions, uint32_t* extension_count) {
    symbol* glfw_extensions = glfwGetRequiredInstanceExtensions(extension_count);
    symbol* result = malloc((*extension_count + 4) * sizeof(char*));
    memcpy(result, glfw_extensions, (*extension_count) * sizeof(char*));

#ifndef WIN32
    result[(*extension_count)++] = "VK_KHR_portability_enumeration";
#endif

#ifdef __APPLE__
    result[(*extension_count)++] = "VK_KHR_get_physical_device_properties2";
#endif

    if (enable_validation) result[(*extension_count)++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    for (int i = 0; i < *extension_count; i++)
        print("instance extension: %s", result[i]);

    *extensions = result;
}

VkInstance vk_create() {
    const char** extensions = null;
    u32          extension_count = 0;
    get_required_extensions(&extensions, &extension_count);
    const char* validation_layer = enable_validation ? "VK_LAYER_KHRONOS_validation" : null;

    VkInstance instance;
    VkResult result = vkCreateInstance(&(VkInstanceCreateInfo) {
            .sType                      = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo           = &(VkApplicationInfo) {
                .sType                  = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName       = "trinity",
                .applicationVersion     = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName            = "trinity",
                .engineVersion          = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion             = vk_version,
            },
            .enabledExtensionCount      = extension_count,
            .ppEnabledExtensionNames    = extensions,
            .enabledLayerCount          = (u32)enable_validation,
            .ppEnabledLayerNames        = &validation_layer
        }, null, &instance);

    verify(result == VK_SUCCESS, "failed to create Vulkan instance, error: %d\n", result);

    _vkCreateDebugUtilsMessengerEXT  = vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    verify(_vkCreateDebugUtilsMessengerEXT, "cannot find vk debug function");
    
    free(extensions);
    return instance;
}

void trinity_init(trinity t) {
    verify(glfwInit(), "glfw init");
    if (!glfwVulkanSupported()) {
        fault("glfw does not support vulkan");
        glfwTerminate();
        return;
    }

    t->instance = vk_create();
    verify(t->instance, "vk instance failure");

    // enumerate physical devices (adapter in WebGPU)
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(t->instance, &deviceCount, null);
    verify(deviceCount > 0, "failed to find GPUs with Vulkan support");

    VkPhysicalDevice physical_devices[deviceCount];
    vkEnumeratePhysicalDevices(t->instance, &deviceCount, physical_devices);
    t->physical_device = physical_devices[0]; // Choose the first device or implement selection logic

    // Check for device extensions
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(t->physical_device, null, &extension_count, null);
    verify(extension_count > 0, "failed to find device extensions");

    VkExtensionProperties extensions[extension_count];
    vkEnumerateDeviceExtensionProperties(t->physical_device, null, &extension_count, extensions);
    
    // Check for required extensions
    bool swapchain_supported = false;

    symbol device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    int total_extension_count = sizeof(device_extensions) / sizeof(device_extensions[0]);
    
    // Track which RT extensions are found
    bool found_extensions[total_extension_count];
    memset(found_extensions, 0, sizeof(found_extensions));
    
    // Check which extensions are available
    for (uint32_t i = 0; i < extension_count; i++) {
        print("[supported] device extension: %s", extensions[i].extensionName);
        if (strcmp(extensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            swapchain_supported = true;
        
        // Check for RT extensions
        for (int j = 0; j < total_extension_count; j++) {
            if (strcmp(extensions[i].extensionName, device_extensions[j]) == 0) {
                found_extensions[j] = true;
                break;
            }
        }
    }

    verify(swapchain_supported, "VK_KHR_swapchain extension is not supported by the physical device");
    
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &bufferDeviceAddressFeatures;

    t->msaa_samples = max_sample_count(t->physical_device);
    vkGetPhysicalDeviceFeatures2(t->physical_device, &features2); // Fetch the features from the physical device

    // Prepare device extensions
    float    queuePriority = 1.0f;
    VkResult result = vkCreateDevice(t->physical_device, &(VkDeviceCreateInfo) {
            .sType                      = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                      = &features2,
            .queueCreateInfoCount       = 1,
            .pQueueCreateInfos          = &(VkDeviceQueueCreateInfo) {
                .sType                  = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex       = 0, // Replace with actual graphics queue family index
                .queueCount             = 1,
                .pQueuePriorities       = &queuePriority,
            },
            .enabledExtensionCount      = 1,
            .ppEnabledExtensionNames    = device_extensions,
        }, null, &t->device);
    verify(result == VK_SUCCESS, "cannot create logical device");

    //_vkCreateAccelerationStructureKHR = 
    //    (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(t->device, "vkCreateAccelerationStructureKHR");
    //_vkGetAccelerationStructureBuildSizesKHR = 
    //    (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(t->device, "vkGetAccelerationStructureBuildSizesKHR");
    
    t->queue_family_index = -1;
}

void trinity_destructor(trinity t) {
    vkDeviceWaitIdle(t->device);
    pairs(t->device_memory, i) {
        VkBuffer      buffer  = (VkBuffer)      i->key;
        VkDeviceMemory memory = (VkDeviceMemory)i->value;
        vkDestroyBuffer(t->device, buffer, null);
        vkFreeMemory   (t->device, memory, null);
    }

    vkDestroyDevice(t->device, null);
    vkDestroyInstance(t->instance, null);
    glfwTerminate();
}

none buffer_init(buffer b) {
    trinity                 t     = b->t;
    VkDeviceSize            size  = b->size;
    VkBufferUsageFlags      usage = 0;
    VkMemoryPropertyFlags   props = 0;
    
    if (b->u_uniform)       usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (b->u_src)           usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (b->u_dst)           usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (b->u_shader)        usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (b->u_storage)       usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (b->u_vertex)        usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (b->u_index)         usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    if (b->m_device_local)  props |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (b->m_host_visible)  props |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    if (b->m_host_coherent) props |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VkBufferCreateInfo buffer_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult result = vkCreateBuffer(t->device, &buffer_info, null, &b->vk_buffer);
    verify(result == VK_SUCCESS, "failed to create buffer");

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(t->device, b->vk_buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(t, mem_requirements.memoryTypeBits, props),
        .pNext           = &(VkMemoryAllocateFlagsInfo) {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
        }
    };

    result = vkAllocateMemory(t->device, &alloc_info, null, &b->vk_memory);
    verify(result == VK_SUCCESS, "failed to allocate buffer memory");

    vkBindBufferMemory(t->device, b->vk_buffer, b->vk_memory, 0);

    // Copy data if provided
    if (b->data) {
        void* mapped_memory = mmap(b);
        memcpy(mapped_memory, b->data, b->size);
        unmap(b);
    }
}

none buffer_update(buffer a, ARef data) {
    ARef m = mmap(a);
    memcpy(m, data, a->size);
    unmap(a);
}

ARef buffer_mmap(buffer b) {
    void* mapped_memory;
    VkResult result = vkMapMemory(b->t->device, b->vk_memory, 0, (VkDeviceSize)b->size, 0, &mapped_memory);
    verify(result == VK_SUCCESS, "failed to map buffer memory");
    return mapped_memory;
}

none buffer_unmap(buffer b) {
    verify(b->vk_memory, "expected memory");
    vkUnmapMemory(b->t->device, b->vk_memory);
}

none buffer_destructor(buffer a) {
    vkDestroyBuffer(a->t->device, a->vk_buffer, null);
    vkFreeMemory   (a->t->device, a->vk_memory, null);
}


define_class(trinity)
define_class(shader)
define_class(pipeline)
define_class(part)
define_class(node)
define_class(model)
define_class(window)
define_class(buffer)
define_class(particle)

define_enum(Surface)
define_class(World)


Node Model_find(Model a, cstr name) {
    each (a->nodes, Node, node)
        if (cmp(node->name, name) == 0)
            return node;
    return (Node)null;
}

Node Model_parent(Model a, Node n) {
    num node_index = index_of(a->nodes, n);
    verify(node_index >= 0, "invalid node memory");
    each (a->nodes->elements, Node, node) {
        for (num i = 0, ln = len(node->children); i < ln; i++) {
            i64 id = 0;//get(node->children, i);
            if (node_index == id)
                return node;
        }
    }
    return (Node)null;
}

int Model_index_of(Model a, cstr name) {
    int index = 0;
    each (a->nodes, Node, node) {
        if (cmp(node->name, name) == 0)
            return index;
        index++;
    }
    return -1;
}

Node Model_index_string(Model a, string name) {
    return find(a, cstring(name));
}

/// builds Transform, separate from Model/Skin/Node and contains usable glm types
Transform Model_node_transform(Model a, JData joints, mat4f parent_mat, int node_index, Transform parent) {
    Node node = a->nodes->elements[node_index];

    node->processed = true;
    Transform transform;
    if (node->joint_index >= 0) {
        transform->jdata     = joints;

        mat4f i = mat4f((floats)null);
        vec3f v = vec3f(1, 1, 1);
        mat4f rotation_m4 = mat4f(&node->rotation); // calls the quaternion constructor on our struct
        /// vectors have no special field on the end so we have to invoke their entire function names (not using macro or function table; people like this better)
        transform->local = mat4f((floats)null);
        transform->local = mat4f_translate(&transform->local, &node->translation);
        transform->local = mat4f_mul      (&transform->local, &rotation_m4);
        transform->local = mat4f_scale    (&transform->local, &node->scale);
        transform->local_default = transform->local;
        transform->iparent   = parent->istate;
        transform->istate    = node->joint_index;
        mat4f state_mat = mat4f_mul(&parent_mat, &transform->local_default);
        mat4f* jstates = data(joints->states);
        jstates[transform->istate] = state_mat;
        each (node->children, object, p_node_index) {
            int node_index = *(int*)p_node_index;
            /// ch is referenced from the ops below, when called here (not released)
            Transform ch = node_transform(a, joints, state_mat, node_index, transform);
            if (ch) {
                Node n = a->nodes->elements[node_index];
                verify(n->joint_index == ch->istate, "joint index mismatch");
                push(transform->ichildren, &ch->istate); /// there are cases where a referenced node is not part of the joints array; we dont add those.
            }
        }
        push(joints->transforms, transform);
    }
    return transform;
}

JData Model_joints(Model a, Node node) {
    if (node->mx_joints)
        return node->mx_joints;

    JData joints;
    mat4f ident = mat4f_ident();
    if (node->skin != -1) {
        Skin skin         = a->skins->elements[node->skin];
        map  all_children = map();
        each (skin->joints, object, p_node_index) {
            int node_index = *(int*)p_node_index;
            Node node = a->nodes->elements[node_index];
            each (node->children, object, i) {
                verify(!contains(all_children, i), "already contained");
                set(all_children, i, A_bool(true));
            }
        }

        int j_index = 0;
        values (skin->joints, int, node_index)
            ((Node)a->nodes->elements[node_index])->joint_index = j_index++;

        int n = len(skin->joints);
        joints->transforms = array_Transform(n); /// we are appending, so dont set size (just verify)
        joints->states     = vector_mat4f   (n);
        
        for (int i = 0; i < n; i++)
            push(joints->states, &ident);
        
        /// for each root joint, resolve the local and global matrices
        values (skin->joints, int, node_index) {
            if (contains(all_children, &node_index)) // fix
                continue;
            
            node_transform(a, joints, ident, node_index, null);
        }
    } else {
        int n = len(joints->states);
        for (int i = 0; i < n; i++)
            push(joints->states, &ident);
    }

    /// adding transforms twice
    verify(len(joints->states) == len(joints->transforms), "length mismatch");
    node->mx_joints = joints;
    return joints;
}

/// Data is Always upper-case, ...sometimes lower
void Accessor_init(Accessor a) {
    a->stride      = vcount(a) * component_size(a);
    a->total_bytes = a->stride * vcount(a);
    verify(a->count, "count is required");
}

u64 Accessor_vcount(Accessor a) {
    u64 vsize = 0;
    switch (a->type) {
        case CompoundType_SCALAR: vsize = 1; break;
        case CompoundType_VEC2:   vsize = 2; break;
        case CompoundType_VEC3:   vsize = 3; break;
        case CompoundType_VEC4:   vsize = 4; break;
        case CompoundType_MAT2:   vsize = 4; break;
        case CompoundType_MAT3:   vsize = 9; break;
        case CompoundType_MAT4:   vsize = 16; break;
        default: fault("invalid CompoundType");
    }
    return vsize;
};

u64 Accessor_component_size(Accessor a) {
    u64 scalar_sz = 0;
    switch (a->componentType) {
        case ComponentType_BYTE:           scalar_sz = sizeof(u8); break;
        case ComponentType_UNSIGNED_BYTE:  scalar_sz = sizeof(u8); break;
        case ComponentType_SHORT:          scalar_sz = sizeof(u16); break;
        case ComponentType_UNSIGNED_SHORT: scalar_sz = sizeof(u16); break;
        case ComponentType_UNSIGNED_INT:   scalar_sz = sizeof(u32); break;
        case ComponentType_FLOAT:          scalar_sz = sizeof(float); break;
        default: fault("invalid ComponentType");
    }
    return scalar_sz;
};

AType Accessor_member_type(Accessor a) {
    switch (a->componentType) {
        case ComponentType_BYTE:
        case ComponentType_UNSIGNED_BYTE:
            switch (a->type) {
                case CompoundType_SCALAR: return typeid(i8);
                //case CompoundType_VEC2:   return typeid(vec2i8);
                //case CompoundType_VEC3:   return typeid(vec3i8);
                //case CompoundType_VEC4:   return typeid(vec4i8);
                default: break;
            }
            break;
        case ComponentType_SHORT:
        case ComponentType_UNSIGNED_SHORT:
            switch (a->type) {
                case CompoundType_SCALAR: return typeid(i16);
                default: break;
            }
            break;
        case ComponentType_UNSIGNED_INT:
            switch (a->type) {
                case CompoundType_SCALAR: return typeid(i32);
                default: break;
            }
            break;
        case ComponentType_FLOAT:
            switch (a->type) {
                case CompoundType_SCALAR: return typeid(f32);
                case CompoundType_VEC2:   return typeid(vec2f);
                case CompoundType_VEC3:   return typeid(vec3f);
                case CompoundType_VEC4:   return typeid(vec4f);
                //case CompoundType_MAT2:   return typeid(mat2f); 
                //case CompoundType_MAT3:   return typeid(mat3f); 
                case CompoundType_MAT4:   return typeid(mat4f); 
                default: fault("invalid CompoundType");
            }
            break;
        default:
            break;
    }
    fault("invalid CompoundType: %o", e_str(CompoundType, a->type));
    return 0;
};

void Transform_multiply(Transform a, mat4f m) {
    a->local = mat4f_mul(&a->local, &m);
    propagate(a);
}

void Transform_set(Transform a, mat4f m) {
    a->local = m;
    propagate(a);
}

void Transform_set_default(Transform a) {
    a->local = a->local_default;
    propagate(a);
}

void Transform_operator__assign_mul(Transform a, mat4f m) {
    multiply(a, m);
}

bool Transformer_cast_bool(Transform a) {
    return a->jdata != null;
}

void Transform_propagate(Transform a) {
    mat4f ident = mat4f_ident(); /// iparent's istate will always == iparent
    mat4f m = (a->iparent != -1) ? *(mat4f*)get(a->jdata->states, a->iparent) : ident;
    mat4f* jstates = data(a->jdata->states);
    jstates[a->istate] = mat4f_mul(&m, &a->local);
    each (a->ichildren, i64*, i)
        propagate(a->jdata->transforms->elements[*i]);
}

Primitive Node_primitive(Node a, Model mdl, cstr name) {
    Mesh m = get(mdl->meshes, a->mesh);
    return m ? primitive(m, name) : null;
}

Primitive Mesh_primitive(Mesh a, cstr name) {
    each (a->primitives, Primitive, p) {
        if (cmp(p->name, name) == 0)
            return p;
    }
    return null;
}


define_enum(ComponentType)
define_enum(CompoundType)
define_enum(TargetType)
define_enum(Mode)
define_enum(Interpolation)

define_class(Sampler)
define_class(ChannelTarget)
define_class(Channel)
define_class(Animation)
define_class(SparseInfo)
define_class(Sparse)
define_class(Accessor)
define_class(BufferView)
define_class(Skin)
define_class(JData)
define_class(Transform)
define_class(Node)
define_class(Primitive)
define_class(MeshExtras)
define_class(Mesh)
define_class(Scene)
define_class(AssetDesc)
define_class(Buffer)
define_class(Model)

define_enum(Pixel)
define_enum(Filter)
define_enum(Polygon)
define_enum(Asset)
define_enum(Sampling)

define_class(pbrMetallicRoughness)
define_class(TextureInfo)
define_class(Material)

//define_struct(HumanVertex)

define_meta(array_Transform,  array, Transform)
define_meta(array_Sampler,    array, Sampler)
define_meta(array_Channel,    array, Channel)
define_meta(array_Primitive,  array, Primitive)
define_meta(array_Node,       array, Node)
define_meta(array_Skin,       array, Skin)
define_meta(array_Accessor,   array, Accessor)
define_meta(array_BufferView, array, BufferView)
define_meta(array_Mesh,       array, Mesh)
define_meta(array_Scene,      array, Scene)
define_meta(array_Material,   array, Material)
define_meta(array_Buffer,     array, Buffer)
define_meta(array_Animation,  array, Animation)
