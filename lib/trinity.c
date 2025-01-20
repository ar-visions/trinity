#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <import>
#include <sys/stat.h>

static const int enable_validation = 1;
static PFN_vkCreateDebugUtilsMessengerEXT  _vkCreateDebugUtilsMessengerEXT;

static void handle_glfw_key(
    GLFWwindow *glfw_window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_R && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    }
}

static u32 find_memory_type(VkPhysicalDevice physical_device, u32 type_filter, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &props);

    for (u32 i = 0; i < props.memoryTypeCount; i++)
        if ((type_filter & (1 << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags)
            return i;

    fault("Failed to find a suitable memory type");
    return UINT32_MAX;
}

static VkBuffer create_buffer(trinity t, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, object data) {
    VkBuffer buffer;
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult result = vkCreateBuffer(t->device, &buffer_info, NULL, &buffer);
    verify(result == VK_SUCCESS, "Failed to create buffer");

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(t->device, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = find_memory_type(t->physical_device, mem_requirements.memoryTypeBits, properties),
    };

    VkDeviceMemory buffer_memory;
    result = vkAllocateMemory(t->device, &alloc_info, NULL, &buffer_memory);
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
    set(t->device_memory, buffer, buffer_memory);
    return buffer;
}

static void update_framebuffers(window w) {
    trinity t = w->t;

    // Wait for the device to idle before resizing
    vkDeviceWaitIdle(t->device);

    // Destroy old framebuffers
    if (w->framebuffers) {
        for (uint32_t i = 0; i < w->image_count; ++i) {
            vkDestroyFramebuffer(t->device, w->framebuffers[i], NULL);
        }
        free(w->framebuffers);
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
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = w->surface_caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = w->present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    vkDestroySwapchainKHR(t->device, w->swapchain, null);
    VkResult result = vkCreateSwapchainKHR(t->device, &swapchain_info, null, &w->swapchain);
    verify(result == VK_SUCCESS, "Failed to create swapchain");

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
    vkGetSwapchainImagesKHR(t->device, w->swapchain, &w->image_count, NULL);

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
    vkGetSwapchainImagesKHR(t->device, w->swapchain, &w->image_count, swapchain_images);

    // Allocate new framebuffers
    w->framebuffers = malloc(w->image_count * sizeof(VkFramebuffer));
    for (uint32_t i = 0; i < w->image_count; ++i) {
        VkResult result;
        VkSemaphoreCreateInfo semaphore_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        result = vkCreateSemaphore(t->device, &semaphore_info, NULL, &w->image_available_semaphore[i]);
        verify(result == VK_SUCCESS, "failed to create image available semaphore");
        result = vkCreateSemaphore(t->device, &semaphore_info, NULL, &w->render_finished_semaphore[i]);
        verify(result == VK_SUCCESS, "failed to create render finished semaphore");

        result = vkCreateFence(t->device, &(VkFenceCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT, // Initially signaled to allow the first use
            }, NULL, &w->command_fences[i]);
        verify(result == VK_SUCCESS, "failed to create fence");

        // Create image view
        VkImageViewCreateInfo image_view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain_images[i],
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

        VkImageView image_view;
        result = vkCreateImageView(t->device, &image_view_info, NULL, &image_view);
        verify(result == VK_SUCCESS, "Failed to create image view");

        // Create framebuffer
        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = w->render_pass, // Assume the render pass is pre-configured
            .attachmentCount = 1,
            .pAttachments = &image_view,
            .width = w->width,
            .height = w->height,
            .layers = 1,
        };

        result = vkCreateFramebuffer(t->device, &framebuffer_info, NULL, &w->framebuffers[i]);
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

void pipeline_init(pipeline p) {
    object data = p->read ? p->read : p->read_write;
    verify(data, "No data provided");
    bool is_comp = !p->read;
    A header = A_header(data);
    AType type = isa(data);
    trinity t = p->t;
    window w = p->w;

    // Vertex count and buffer size
    p->vertex_count = header->count;
    p->total_size = type->size * p->vertex_count;

    // Create buffer if it doesn't exist
    if (contains(t->buffers, data)) {
        p->buffer = get(t->buffers, data);
    } else {
        VkBufferUsageFlags usage = 
            (is_comp ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0) |
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
                VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VkBuffer buffer = create_buffer(t, p->total_size, usage,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, data);

        // bind data (key) with buffer (value)
        set(t->buffers, data, buffer);
        p->buffer = buffer;
    }

    // Pipeline Layout (Bindings and Layouts)
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0,
    };

    VkPipelineLayout pipeline_layout;
    VkResult result = vkCreatePipelineLayout(t->device, &pipeline_layout_info, NULL, &pipeline_layout);
    verify(result == VK_SUCCESS, "Failed to create pipeline layout");
    p->layout = pipeline_layout;

    if (p->read) {
        // Graphics Pipeline
        VkVertexInputBindingDescription binding_description = {
            .binding = 0,
            .stride = type->size,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };

        int attr_count = 0;
        VkVertexInputAttributeDescription* attributes = calloc(type->member_count, sizeof(VkVertexInputAttributeDescription));
        int offset = 0;
        for (int i = 0; i < type->member_count; i++) {
            Member mem = &type->members[i];
            if (mem->member_type == A_TYPE_INLAY || mem->member_type == A_TYPE_PROP) {
                attributes[attr_count].binding = 0;
                attributes[attr_count].location = attr_count;
                attributes[attr_count].format = 
                    (mem->type == typeid(v2)) ? VK_FORMAT_R32G32_SFLOAT :
                    (mem->type == typeid(v3)) ? VK_FORMAT_R32G32B32_SFLOAT :
                    (mem->type == typeid(v4)) ? VK_FORMAT_R32G32B32A32_SFLOAT : 
                                                VK_FORMAT_UNDEFINED;
                attributes[attr_count].offset = offset;
                offset += mem->type->size;
                attr_count++;
            }
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

        // Set up shaders (assume pre-compiled SPIR-V shaders are used)
        VkPipelineShaderStageCreateInfo vert_shader_stage_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = p->shader->vert_module,
            .pName = "main"
        };

        VkPipelineShaderStageCreateInfo frag_shader_stage_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = p->shader->frag_module,
            .pName = "main"
        };

        VkPipelineShaderStageCreateInfo shader_stages[] = {
            vert_shader_stage_info, frag_shader_stage_info
        };

        // Add viewport and rasterization states (example setup)
        VkPipelineViewportStateCreateInfo viewport_state = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount          = 1,
            .pViewports             = NULL, // Set dynamically
            .scissorCount           = 1,
            .pScissors              = NULL,  // Set dynamically
        };

        VkPipelineRasterizationStateCreateInfo rasterization_state = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable       = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode            = VK_POLYGON_MODE_FILL,
            .lineWidth              = 1.0f,
            .cullMode               = VK_CULL_MODE_NONE, //VK_CULL_MODE_BACK_BIT,
            .frontFace              = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable        = VK_FALSE,
        };

        VkPipelineMultisampleStateCreateInfo multisample_state = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples   = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable    = VK_FALSE,
        };

        VkPipelineColorBlendAttachmentState color_blend_attachment = {
            .blendEnable            = VK_FALSE,
            .colorWriteMask         = VK_COLOR_COMPONENT_R_BIT |
                                      VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT |
                                      VK_COLOR_COMPONENT_A_BIT,
        };

        VkPipelineColorBlendStateCreateInfo color_blend_state = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable          = VK_FALSE,
            .attachmentCount        = 1,
            .pAttachments           = &color_blend_attachment,
        };

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            // Add more dynamic states if needed (e.g., VK_DYNAMIC_STATE_LINE_WIDTH)
        };

        VkPipelineDynamicStateCreateInfo dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]),
            .pDynamicStates = dynamic_states,
        };

        // Finally, create the pipeline
        VkGraphicsPipelineCreateInfo pipeline_info = {
            .sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount             = 2,
            .pStages                = shader_stages,
            .pVertexInputState      = &vertex_input_info,
            .pInputAssemblyState    = &input_assembly_info,
            .pViewportState         = &viewport_state,
            .pRasterizationState    = &rasterization_state,
            .pMultisampleState      = &multisample_state,
            .pColorBlendState       = &color_blend_state,
            .pDynamicState          = &dynamic_state,
            .layout                 = p->layout,
            .renderPass             = w->render_pass,
            .subpass                = 0,
            .basePipelineHandle     = VK_NULL_HANDLE,
        };

        result = vkCreateGraphicsPipelines(t->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &p->render);
        verify(result == VK_SUCCESS, "pipeline creation fail");

        free(attributes);
    } else {
        // Compute Pipeline
        VkComputePipelineCreateInfo compute_pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .layout = pipeline_layout,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = p->shader->comp_module,
                .pName = "main",
            },
        };

        VkPipeline compute_pipeline;
        result = vkCreateComputePipelines(t->device, VK_NULL_HANDLE, 1, &compute_pipeline_info, NULL, &compute_pipeline);
        verify(result == VK_SUCCESS, "Failed to create compute pipeline");
        p->compute = compute_pipeline;
    }
}

void pipeline_destructor(pipeline p) {
    vkDestroyPipelineLayout(p->t->device, p->layout, null);
    if (p->render)  vkDestroyPipeline(p->t->device, p->render, null);
    if (p->compute) vkDestroyPipeline(p->t->device, p->compute, null);
}


void window_push(window w, pipeline p) {
    array a = w->pipelines;
    push(a, (object)p);
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
    vkGetPhysicalDeviceQueueFamilyProperties(t->physical_device, &queue_family_count, NULL);
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

    w->pipelines = array();
    // Initialize GLFW window with Vulkan compatibility
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    w->window = glfwCreateWindow(w->width, w->height, "triangle [Vulkan + GLFW]", NULL, NULL);
    verify(w->window, "Failed to create GLFW window");

    glfwSetWindowUserPointer(w->window, (void *)w);
    glfwSetKeyCallback(w->window, handle_glfw_key);
    glfwSetFramebufferSizeCallback(w->window, handle_glfw_framebuffer_size);

    // Create Vulkan surface
    VkResult result = glfwCreateWindowSurface(t->instance, w->window, NULL, &w->surface);
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
            }, NULL, &t->debug);
        verify(result == VK_SUCCESS, "debug messenger");

        /// buffer management for pipeline, without management (it does not drop/hold this external data; not A-type)
        t->device_memory = map(unmanaged, true);
        t->buffers       = map(unmanaged, true);
    }
    
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = t->queue_family_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    result = vkCreateCommandPool(t->device, &pool_info, NULL, &t->command_pool);
    verify(result == VK_SUCCESS, "failed to create command pool");


    // Query surface capabilities
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(t->physical_device, w->surface, &w->surface_caps);
    verify(result == VK_SUCCESS, "Failed to query surface capabilities");

    // Query supported surface formats
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(t->physical_device, w->surface, &format_count, NULL);
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
    vkGetPhysicalDeviceSurfacePresentModesKHR(t->physical_device, w->surface, &present_mode_count, NULL);
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
        w->extent.width = max(w->surface_caps.minImageExtent.width,
                              min(w->surface_caps.maxImageExtent.width, w->extent.width));
        w->extent.height = max(w->surface_caps.minImageExtent.height,
                               min(w->surface_caps.maxImageExtent.height, w->extent.height));
    }

    // Create the render pass
    VkAttachmentDescription color_attachment = {
        .format = w->surface_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    result = vkCreateRenderPass(t->device, &render_pass_info, NULL, &w->render_pass);
    verify(result == VK_SUCCESS, "Failed to create render pass");

    update_framebuffers(w);  // Call the framebuffer update directly
}

void window_loop(window w) {
    trinity t = w->t;
    int semaphore_frame = 0;
    int last_fence = -1;
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
            .clearValueCount    = 1,
            .pClearValues       = &(VkClearValue) { .color = { .float32 = { 0.2f, 0.0f, 0.5f, 1.0f }}}
        }, VK_SUBPASS_CONTENTS_INLINE);

        // Bind and execute pipelines
        // a pipeline could have compute and render in one, if we wanted to use it this way
        // shaders have compute/vert/frag, and thus may be integrated by pipeline
        each(w->pipelines, pipeline, p) {
            if (p->compute) {
                // For compute pipelines, use a separate compute pass
                vkCmdBindPipeline(frame, VK_PIPELINE_BIND_POINT_COMPUTE, p->compute);
                vkCmdBindDescriptorSets(frame, VK_PIPELINE_BIND_POINT_COMPUTE, p->layout, 0, 1, &p->bind, 0, NULL);
                vkCmdDispatch(frame, p->vertex_count, 1, 1);
            }
            if (p->render) {
                // For render pipelines, use the active render pass
                vkCmdBindPipeline(frame, VK_PIPELINE_BIND_POINT_GRAPHICS, p->render);
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(frame, 0, 1, &p->buffer, offsets);
                vkCmdDraw(frame, p->vertex_count, 1, 0, 0);
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
}

void window_destructor(window w) {
    trinity t = w->t;
    for (int i = 0; i < w->image_count; i++) {
        vkDestroySemaphore(t->device, w->image_available_semaphore[i], null);
        vkDestroySemaphore(t->device, w->render_finished_semaphore[i], null);
        vkDestroyFence    (t->device, w->command_fences[i], null);
    }
    
    free(w->command_buffers);
    free(w->command_fences);
    free(w->image_available_semaphore);
    free(w->render_finished_semaphore);
    vkDestroySwapchainKHR(t->device, w->swapchain, null);
    vkDestroySurfaceKHR(w->t->instance, w->surface, null);
    glfwDestroyWindow  (w->window);
}

void shader_init(shader s) {
    // Generate .spv for resources provided in shader (frag, vert and comp)
    string spv_file;
    bool found = false;
    for (int i = 0; i < 3; i++) {
        string name = i == 0 ? s->vert : i == 1 ? s->frag : s->comp;
        if (!name || !file_exists("%o", name)) continue;
        spv_file = format("%o.spv", name);
        found = true;

        // Check if the .spv file already exists and is up to date
        struct stat source_stat, spv_stat;
        int spv_exists = stat(spv_file->chars, &spv_stat) == 0;

        /// if no spv, or our source is newer than the binary (we have changed it!)
        if (!spv_exists || source_stat.st_mtime > spv_stat.st_mtime) {
            // Compile GLSL to SPIR-V using glslangValidator
            string command = format("glslangValidator -V %o -o %o", name, spv_file);
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
        VkResult vkResult = vkCreateShaderModule(s->t->device, &createInfo, NULL, &module);
        if (vkResult != VK_SUCCESS) {
            print("Failed to create Vulkan shader module");
            free(spirv);
            exit(1);
        }

        if (i == 0) s->vert_module = module;
        if (i == 1) s->frag_module = module;
        if (i == 2) s->comp_module = module;

        free(spirv);
        print("shader module created: %o", spv_file);
    }
    verify(found, "shader not found: %o %o %o", s->vert, s->frag, s->comp);
}




void shader_destructor(shader s) {
    vkDestroyShaderModule(s->t->device, s->vert_module, null);
    vkDestroyShaderModule(s->t->device, s->frag_module, null);
    vkDestroyShaderModule(s->t->device, s->comp_module, null);
}


void get_required_extensions(const char*** extensions, uint32_t* extension_count) {
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(extension_count);
    const char** result = malloc((*extension_count + 4) * sizeof(char*));
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
    const char** extensions = NULL;
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
                .apiVersion             = VK_API_VERSION_1_2,
            },
            .enabledExtensionCount      = extension_count,
            .ppEnabledExtensionNames    = extensions,
            .enabledLayerCount          = (u32)enable_validation,
            .ppEnabledLayerNames        = &validation_layer
        }, NULL, &instance);

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
    vkEnumeratePhysicalDevices(t->instance, &deviceCount, NULL);
    verify(deviceCount > 0, "failed to find GPUs with Vulkan support");

    VkPhysicalDevice physical_devices[deviceCount];
    vkEnumeratePhysicalDevices(t->instance, &deviceCount, physical_devices);
    t->physical_device = physical_devices[0]; // Choose the first device or implement selection logic

    // verify required device extensions
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(t->physical_device, NULL, &extension_count, NULL);
    verify(extension_count > 0, "failed to find device extensions");

    VkExtensionProperties extensions[extension_count];
    vkEnumerateDeviceExtensionProperties(t->physical_device, NULL, &extension_count, extensions);
    bool swapchain_supported = false;
    for (uint32_t i = 0; i < extension_count; i++) {
        print("[supported] device extension: %s", extensions[i].extensionName);
        if (strcmp(extensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            swapchain_supported = true;
            break;
        }
    }
    verify(swapchain_supported, "VK_KHR_swapchain extension is not supported by the physical device");

    float       queuePriority = 1.0f;
    const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkResult result = vkCreateDevice(t->physical_device, &(VkDeviceCreateInfo) {
            .sType                      = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount       = 1,
            .pQueueCreateInfos          = &(VkDeviceQueueCreateInfo) {
                .sType                  = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex       = 0, // Replace with actual graphics queue family index
                .queueCount             = 1,
                .pQueuePriorities       = &queuePriority,
            },
            .enabledExtensionCount      = sizeof(device_extensions) / sizeof(device_extensions[0]),
            .ppEnabledExtensionNames    = device_extensions,
        }, NULL, &t->device);
    verify(result == VK_SUCCESS, "cannot create logical device");

    t->queue_family_index = UINT32_MAX;
}

void trinity_destructor(trinity t) {
    vkDeviceWaitIdle(t->device);
    pairs(t->device_memory, i) {
        VkBuffer      buffer  = (VkBuffer)      i->key;
        VkDeviceMemory memory = (VkDeviceMemory)i->value;
        vkDestroyBuffer(t->device, buffer, NULL);
        vkFreeMemory   (t->device, memory, NULL);
    }

    vkDestroyDevice(t->device, NULL);
    vkDestroyInstance(t->instance, NULL);
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


/// builds Transform, separate from Model/Skin/Node and contains usable glm types
Transform Model_node_transform(Model data, JData joints, mat4f parent_mat, int node_index, Transform parent) {
    Node node = data->nodes->elements[node_index];

    //assert(node->processed == false);

    node->processed = true;
    Transform transform;
    if (node->joint_index >= 0) {
        transform->jdata     = joints;

        mat4f i = mat4f(null);
        vec3f v = vec3f(1, 1, 1);
        struct mat4f_f* table = isa(i);

        transform->local     =     mat4f(null);
        transform->local     = translate(transform->local, node->translation);
        transform->local     =       mul(transform->local, mat4f(node->rotation));
        transform->local     =     scale(transform->local, node->scale);
        transform->local_default = transform->local;
        transform->iparent   = parent->istate;
        transform->istate    = node->joint_index;
        mat4f state_mat = joints->states->elements[transform->istate] = mul(parent_mat, transform->local_default);


        each (node->children, object, p_node_index) {
            int node_index = *(int*)p_node_index;
            /// ch is referenced from the ops below, when called here (not released)
            Transform ch = node_transform(data, joints, state_mat, node_index, transform);
            if (ch) {
                Node n = data->nodes->elements[node_index];
                verify(n->joint_index == ch->istate, "joint index mismatch");
                push(transform->ichildren, A_i64(ch->istate)); /// there are cases where a referenced node is not part of the joints array; we dont add those.
            }
        }

        push(joints->transforms, transform);
    }
    return transform;
}


JData Model_joints(Model data, Node node) {
    if (node->mx_joints)
        return node->mx_joints;

    JData joints;
    if (node->skin != -1) {
        Skin skin         = data->skins[node->skin];
        map  all_children = map();
        each (skin->joints, object, p_node_index) {
            int node_index = *(int*)p_node_index;
            Node node = data->nodes[node_index];
            each (node->children, object, i) {
                verify(!contains(all_children, i), "already contained");
                set(all_children, i, A_bool(true));
            }
        }

        int j_index = 0;
        values (skin->joints, int, node_index)
            data->nodes[node_index]->joint_index = j_index++;

        joints->transforms = array_Transform(skin->joints.len()); /// we are appending, so dont set size (just verify)
        joints->states     = array_m44f     (skin->joints.len());
        fill(joints->states, m44f(0));
        Transform null;
        /// for each root joint, resolve the local and global matrices
        values (skin->joints, int, node_index) {
            if (contains(all_children, p_node_index))
                continue;
            
            m44f      ident = m44f(1.0f);
            node_transform(data, joints, ident, node_index, null);
        }
    }
    /// test code!
    /// better way to test is to only store joints for a specific set tied to 'Eye Target'
    /// todo: Verify Eye Target has influence by its node index in the 'joints' array
    for (m44f &state: joints->states) {
        int test;
        test++;
        state = m44f(1.0f);
    }

    /// adding transforms twice
    assert(joints->states.len() == joints->transforms.len());
    node->mx_joints = joints;
    return joints;
}

define_meta(array_i64,        array, i64)
define_meta(array_Sampler, array, Sampler)
define_meta(array_Sampler,    array, Sampler)
define_meta(array_Channel,    array, Channel)
define_meta(array_map,        array, Maps)
define_meta(array_Primitive,  array, Primitive)
define_meta(array_Node,       array, Node)
define_meta(array_Skin,       array, Skin)
define_meta(array_Accessor,   array, Accessor)
define_meta(array_BufferView, array, BufferView)
define_meta(array_Mesh,       array, Mesh)
define_meta(array_Scene,      array, Scene)
define_meta(array_Buffer,     array, Buffer)
define_meta(array_Animation,  array, Animation)


/*
    struct SparseInfo:mx {
        struct M {
            u64          bufferView;
            ComponentType   componentType;
            /// componentType:
            /// sub-accessor type for bufferView, only for index types
            /// for value types, we are using the componentType of the accessor
            /// we assert that its undefined or set to the same
            properties meta() {
                return {
                    prop { "bufferView",     bufferView    },
                    prop { "componentType",  componentType }
                };
            }
        };
        mx_basic(SparseInfo);
    };

    struct Sparse:mx {
        struct M {
            u64        count;
            SparseInfo    indices;
            SparseInfo    values;
            ///
            properties meta() {
                return {
                    prop { "count",          count       },
                    prop { "indices",        indices     },
                    prop { "values",         values      }
                };
            }
        };
        mx_basic(Sparse);
    };

    struct Accessor:mx {
        struct M {
            u64        bufferView;
            ComponentType componentType;
            CompoundType  type;
            size_t        count;
            Array<float>  min;
            Array<float>  max;
            Sparse        sparse;
            ///
            properties meta() {
                return {
                    prop { "bufferView",     bufferView    },
                    prop { "componentType",  componentType },
                    prop { "count",          count         },
                    prop { "type",           type          },
                    prop { "min",            min           },
                    prop { "max",            max           },
                    prop { "sparse",         sparse        } /// these are overlays we use to make a copied buffer instance with changes made to it
                };
            }

            size_t vcount() {
                size_t vsize = 0;
                switch (type) {
                    case gltf::CompoundType::SCALAR: vsize = 1; break;
                    case gltf::CompoundType::VEC2:   vsize = 2; break;
                    case gltf::CompoundType::VEC3:   vsize = 3; break;
                    case gltf::CompoundType::VEC4:   vsize = 4; break;
                    case gltf::CompoundType::MAT2:   vsize = 4; break;
                    case gltf::CompoundType::MAT3:   vsize = 9; break;
                    case gltf::CompoundType::MAT4:   vsize = 16; break;
                    default: assert(false);
                }
                return vsize;
            };

            size_t component_size() {
                size_t scalar_sz = 0;
                switch (componentType) {
                    case gltf::ComponentType::BYTE:           scalar_sz = sizeof(u8); break;
                    case gltf::ComponentType::UNSIGNED_BYTE:  scalar_sz = sizeof(u8); break;
                    case gltf::ComponentType::SHORT:          scalar_sz = sizeof(u16); break;
                    case gltf::ComponentType::UNSIGNED_SHORT: scalar_sz = sizeof(u16); break;
                    case gltf::ComponentType::UNSIGNED_INT:   scalar_sz = sizeof(u32); break;
                    case gltf::ComponentType::FLOAT:          scalar_sz = sizeof(float); break;
                    default: assert(false);
                }
                return scalar_sz;
            };
        };

        size_t vcount() {
            return data->vcount();
        };

        size_t component_size() {
            return data->component_size();
        }

        mx_basic(Accessor);
    };

    struct BufferView:mx {
        struct M {
            size_t      buffer;
            size_t      byteLength;
            size_t      byteOffset;
            TargetType  target;
            ///
            properties meta() {
                return {
                    prop { "buffer",       buffer       },
                    prop { "byteLength",   byteLength   },
                    prop { "byteOffset",   byteOffset   },
                    prop { "target",       target       }
                };
            }
        };
        mx_basic(BufferView);
    };

    struct Skin:mx {
        struct M {
            string             name;
            Array<int>      joints; /// references nodes!
            int             inverseBindMatrices; /// buffer-view index of mat44 (before index ordering) (assert data type must be matrix 4x4)
            mx              extras;
            mx              extensions;
            ///
            properties meta() {
                return {
                    prop { "name",                name        },
                    prop { "joints",              joints      },
                    prop { "inverseBindMatrices", inverseBindMatrices },
                    prop { "extensions",          extensions  },
                    prop { "extras",              extras      }
                };
            }
        };
        mx_basic(Skin);
    };

    struct JData;
    struct Node;
    struct Transform:mx {
        struct M {
            JData              *jdata;
            int                 istate;
            m44f                local;
            m44f                local_default;
            int                 iparent;
            Array<int>          ichildren; /// all of these are added into Joints::transforms (as well as root Transforms)
        };

        void multiply(const m44f &m) {
            data->local *= m;
            propagate();
        }

        void set(const m44f &m) {
            data->local = m;
            propagate();
        }

        void set_default() {
            data->local = data->local_default;
            propagate();
        }

        void propagate();

        void operator*=(const m44f &m) {
            multiply(m);
        }

        operator bool() { return data->jdata; } /// useful for parent & null state handling

        mx_basic(Transform);
    };

    struct JData {
        Array<m44f>         states;     /// wgpu::Buffer updated with this information per frame
        Array<Transform>    transforms; /// same identity as joints array in skin
    };

    /// references JData
    void Transform::propagate() {
        static m44f ident(1.0f); /// iparent's istate will always == iparent
        m44f &m = (data->iparent != -1) ? data->jdata->states[data->iparent] : ident;
        data->jdata->states[data->istate] = m * data->local;
        for (int &t: data->ichildren.elements<int>())
            data->jdata->transforms[t].propagate();
    }

    struct Joints:mx {
        memory *copy() const {
            Joints joints;
            joints->states = Array<m44f     >(data->states.len());
            memcpy(joints->states.data<m44f>(), data->states.data<m44f>(), sizeof(m44f) * data->states.len());
            joints->states.set_size(data->states.len());

            /// fix mx::copy()
            joints->transforms = Array<Transform>(data->transforms.len());
            memcpy(joints->transforms.data<Transform>(),
                     data->transforms.data<Transform>(),
                     sizeof(Transform) * data->transforms.len());
            joints->transforms.set_size(data->transforms.len());

            for (Transform &transform: joints->transforms.elements<Transform>()) {
                transform->jdata = joints.data;
            }
            return hold(joints);
        }
        size_t total_size() { return data->states.total_size(); }
        size_t count()      { return data->states.len(); }

        Transform &operator[](size_t joint_index) {
            return data->transforms[joint_index];
        }
        mx_object(Joints, mx, JData)
    };

    struct Node:mx {
        struct M {
            string             name;
            int             skin        = -1; /// armature index
            int             mesh        = -1; /// mesh index; this is sometimes not set when there are children referenced and its a group-only
            Array<float>    translation = { 0.0, 0.0, 0.0 };
            Array<float>    rotation    = { 0.0, 0.0, 0.0, 1.0 };
            Array<float>    scale       = { 1.0, 1.0, 1.0 };
            Array<float>    weights     = { }; /// no weights, for no vertices
            Array<int>      children;
            int             joint_index = -1;
            bool            processed;
            mx              mx_joints;      /// if we have assembled joints for a previous Graphics object, we can retreive it here
            bool test;

            ///
            properties meta() {
                return {
                    prop { "name",          name        },
                    prop { "mesh",          mesh        },
                    prop { "skin",          skin        }, /// armature index: Models::skins 
                    prop { "children",      children    },
                    prop { "translation",   translation }, /// apply first
                    prop { "rotation",      rotation    }, /// apply second
                    prop { "scale",         scale       }, /// apply third [these are done on load after json is read in; this is only done in cases where ONE of these are set (we want to set defaults of 1,1,1 for scale, 0,0,0 for translate, etc)]
                    prop { "weights",       weights     }
                };
            }
        };

        mx_basic(Node);
    };

    struct Primitive:mx {
        struct M {
            map          attributes; /// Vertex Attribute Type -> index into accessor
            size_t           indices;
            int              material = -1;
            Mode             mode;
            Array<map>   targets; /// (optional) accessor id for attribute name
            ///
            properties meta() {
                return {
                    prop { "attributes",    attributes },
                    prop { "indices",       indices    },
                    prop { "material",      material   },
                    prop { "mode",          mode       },
                    prop { "targets",       targets    }
                };
            }
        };
        mx_basic(Primitive);
    };

    struct MeshExtras:mx {
        struct M {
            Array<string>       target_names;
            ///
            properties meta() {
                return {
                    prop { "targetNames",  target_names }
                };
            }
        };
        mx_basic(MeshExtras);
    };

    struct Mesh:mx {
        struct M {
            str              name;
            Array<Primitive> primitives;
            Array<float>     weights;
            MeshExtras       extras;
            ///
            properties meta() {
                return {
                    prop { "name",         name       },
                    prop { "primitives",   primitives },
                    prop { "weights",      weights    },
                    prop { "extras",       extras     }
                };
            }
        };
        mx_basic(Mesh);
    };

    struct Scene:mx {
        struct M {
            str              name;
            Array<size_t>    nodes;
            ///
            properties meta() {
                return {
                    prop { "name",   name  },
                    prop { "nodes",  nodes }
                };
            }
        };
        mx_basic(Scene);
    };

    struct AssetDesc:mx {
        struct M {
            str generator;
            str copyright;
            str version;
            ///
            properties meta() {
                return {
                    prop { "generator", generator },
                    prop { "copyright", copyright },
                    prop { "version",   version   }
                };
            }
        };
        mx_basic(AssetDesc);
    };

    struct Buffer:mx {
        struct M {
            size_t    byteLength;
            Array<u8> uri;
            ///
            properties meta() {
                return {
                    prop { "byteLength", byteLength },
                    prop { "uri",        uri        }
                };
            }
        };
        mx_basic(Buffer);
    };

    struct Model:mx {
        struct M {
            Array<Node>       nodes;
            Array<Skin>       skins;
            Array<Accessor>   accessors;
            Array<BufferView> bufferViews;
            Array<Mesh>       meshes;
            size_t            scene; // default scene
            Array<Scene>      scenes;
            AssetDesc         asset;
            Array<Buffer>     buffers;
            Array<Animation>  animations;
            ///
            properties meta() {
                return {
                    prop { "nodes",       nodes },
                    prop { "skins",       skins },
                    prop { "accessors",   accessors },
                    prop { "bufferViews", bufferViews },
                    prop { "meshes",      meshes },
                    prop { "scene",       scene },
                    prop { "scenes",      scenes },
                    prop { "asset",       asset },
                    prop { "buffers",     buffers },
                    prop { "animations",  animations }
                };
            }
        };
        mx_basic(Model);

        static Model load(path p) { return p.read<Model>(); }

        Node *find(str name) {
            for (Node &node: data->nodes.elements<Node>()) {
                if (node->name != name) continue;
                return &node;
            }
            return (Node*)null;
        }

        Node *parent(Node &n) {
            int node_index = -1;
            for (Node &node: data->nodes.elements<Node>()) {
                if (node == n)
                    break;
                node_index++;
            }
            for (Node &node: data->nodes.elements<Node>()) {
                if (node->children.index_of(node_index) >= 0)
                    return &node;
            }
            return (Node*)null;
        }

        int index_of(str name) {
            int index = 0;
            for (Node &node: data->nodes.elements<Node>()) {
                if (node->name == name)
                    return index;
                index++;
            }
            return -1;
        }

        Node &operator[](str name) { return *find(name); }

        Joints joints(Node &node) {
            if (node->mx_joints)
                return node->mx_joints;

            int i_eye_target = index_of("Eye Target");
            int i_eye_target_n = index_of("Eye Target Near");
            bool found_eye_target = false;
            bool found_eye_target_n = false;

            Joints joints;
            
            if (node->skin != -1) {
                Skin &skin = data->skins[node->skin];

                ion::uniques<int> all_children; /// pre-alloc'd at the size of nodes array
                for (int &node_index: skin->joints.elements<int>()) {
                    Node &node = data->nodes[node_index];
                    for (int i: node->children.elements<int>()) {
                        assert(!all_children.contains(i));
                        all_children.set(i);
                    }
                }

                int j_index = 0;
                for (int &node_index: skin->joints)
                    data->nodes[node_index]->joint_index = j_index++;

                joints->transforms = Array<Transform>(skin->joints.len()); /// we are appending, so dont set size (just verify)
                joints->states     = Array<m44f     >(skin->joints.len());
                joints->states    .set_size(skin->joints.len());
                Transform null;
                /// for each root joint, resolve the local and global matrices
                for (int &node_index: skin->joints) {
                    if (node_index == i_eye_target)
                        found_eye_target = true;
                    if (node_index == i_eye_target_n)
                        found_eye_target_n = true;
                        
                    if (all_children.contains(node_index))
                        continue;
                    
                    m44f      ident = m44f(1.0f);
                    node_transform(joints, ident, node_index, null);
                }
            }
            /// test code!
            /// better way to test is to only store joints for a specific set tied to 'Eye Target'
            /// todo: Verify Eye Target has influence by its node index in the 'joints' array
            for (m44f &state: joints->states) {
                int test;
                test++;
                state = m44f(1.0f);
            }

            /// adding transforms twice
            assert(joints->states.len() == joints->transforms.len());
            node->mx_joints = joints;
            return joints;
        }

        protected:

        /// builds Transform, separate from Model/Skin/Node and contains usable glm types
        Transform node_transform(Joints joints, const m44f &parent_mat, int node_index, Transform &parent) {
            Node &node = data->nodes[node_index];
            //assert(node->processed == false);

            node->processed = true;
            Transform transform;
            if (node->joint_index >= 0) {
                transform->jdata     = joints.data;
                transform->local     = m44f(1.0f);
                
                quatf q(
                    node->rotation[3], node->rotation[0],
                    node->rotation[1], node->rotation[2]);
                
                transform->local     = translate(transform->local, vec3f(node->translation[0], node->translation[1], node->translation[2]));
                transform->local    *= m44f(q);
                transform->local     = scale    (transform->local, vec3f(node->scale[0], node->scale[1], node->scale[2]));
                transform->local_default = transform->local;
                transform->iparent   = parent->istate;
                transform->istate    = node->joint_index;
                m44f      &state_mat = joints->states[transform->istate] = parent_mat * transform->local_default;

                for (int node_index: node->children) {
                    /// ch is referenced from the ops below, when called here (not released)
                    Transform ch = node_transform(joints, state_mat, node_index, transform);
                    if (ch) {
                        assert(data->nodes[node_index]->joint_index == ch->istate);
                        transform->ichildren += ch->istate; /// there are cases where a referenced node is not part of the joints array; we dont add those.
                    }
                }

                joints->transforms += transform;
            }
            return transform;
        }

    };
};
*/