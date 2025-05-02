#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <import>
#include <sys/stat.h>
#include <math.h>
#include <opencv.h>

const int enable_validation = 1;
PFN_vkCreateDebugUtilsMessengerEXT  _vkCreateDebugUtilsMessengerEXT;
u32 vk_version = VK_API_VERSION_1_2;


VkSampleCountFlagBits max_sample_count(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device, &props);
    VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts &
                                props.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
    if (counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

u32 find_memory_type(trinity t, u32 type_filter, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(t->physical_device, &props);

    for (u32 i = 0; i < props.memoryTypeCount; i++)
        if ((type_filter & (1 << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags)
            return i;

    fault("could not find memory type");
    return UINT32_MAX;
}

VkDeviceMemory device_memory(trinity t, VkBuffer b) {
    pairs (t->device_memory, i) {
        if (i->key == (object)b) return (VkDeviceMemory)i->value;
    }
    fault("device memory not resolved");
    return null;
}

void transition_image_layout(trinity, VkImage, VkImageLayout, VkImageLayout, int, int, int, int);
void buffer_get_image_bits(buffer b, VkImage image, uint32_t width, uint32_t height);

void transition_image_layout(
    trinity t,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    int baseMipLevel,
    int levelCount,
    int base_array_layer,
    int layer_count
) {
    command cmd = command(t, t);
    begin(cmd);

    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = oldLayout,
        .newLayout           = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = {
            .aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel     = baseMipLevel,
            .levelCount       = levelCount,
            .baseArrayLayer   = base_array_layer,
            .layerCount       = layer_count
        }
    };

    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT; // lame how there is an option here -- so one needs tk now about COLOR ATTACHMENT use-case for GENERAL (which is ambiguous in design)
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // lame how there is an option here -- so one needs tk now about COLOR ATTACHMENT use-case for GENERAL (which is ambiguous in design)
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR      && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        
    } else {
        fault("unsupported layout transition");
    }

    vkCmdPipelineBarrier(
        cmd->vk,
        srcStage, dstStage,
        0, 0, null, 0, null, 1, &barrier
    );

    submit(cmd);
    drop(cmd);
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

static void handle_glfw_key(
    GLFWwindow *glfw_window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_R && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    }
    if (action == GLFW_PRESS && (key == GLFW_KEY_UP || key == GLFW_KEY_DOWN)) {
        window w = glfwGetWindowUserPointer(glfw_window);
        w->debug_value += (key == GLFW_KEY_UP) ? 1.0 : -1.0;
    }
}
 
void create_color_image(window w, VkImageViewCreateInfo* image_view_info) {
    trinity t = w->t;
    /// create color image (for msaa)
    VkImageCreateInfo color_image_info = {
        .sType          = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType      = VK_IMAGE_TYPE_2D,
        .format         = w->surface_format.format,
        .extent         = { w->width, w->height, 1 },
        .mipLevels      = 1,
        .arrayLayers    = 1,
        .samples        = t->msaa_samples,
        .tiling         = VK_IMAGE_TILING_OPTIMAL,
        .usage          = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | 
            (w->backbuffer ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT),
        .sharingMode    = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED
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
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 1, 0, 1);
}


void create_resolve_image(window w, VkImageViewCreateInfo* image_view_info) {
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

    vkCreateImage(t->device, &resolve_info, null, &w->resolve_image);
    VkMemoryRequirements resolve_reqs;
    vkGetImageMemoryRequirements(t->device, w->resolve_image, &resolve_reqs);
    VkMemoryAllocateInfo resolve_alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = resolve_reqs.size,
        .memoryTypeIndex = find_memory_type(t, resolve_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    vkAllocateMemory(t->device, &resolve_alloc, null, &w->resolve_memory);
    vkBindImageMemory(t->device, w->resolve_image, w->resolve_memory, 0);

    // Create resolve image view
    image_view_info->image = w->resolve_image;
    vkCreateImageView(t->device, image_view_info, null, &w->resolve_view);

    transition_image_layout(t, w->resolve_image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 1, 0, 1);
}

void window_resize(window w, i32 width, i32 height) {
    trinity t = w->t;

    w->width  = width;
    w->height = height;
    w->extent.width  = width;
    w->extent.height = height;

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

    if (!w->backbuffer) {
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
    }

    VkResult result;
    if (!w->backbuffer) {
        // Recreate the swapchain with new dimensions
        VkSwapchainCreateInfoKHR swapchain_info = {
            .sType              = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface            = w->surface,
            .minImageCount      = w->surface_caps->minImageCount,
            .imageFormat        = w->surface_format.format,
            .imageColorSpace    = w->surface_format.colorSpace,
            .imageExtent        = {
                .width          = (uint32_t)w->width,
                .height         = (uint32_t)w->height,
            },
            .imageArrayLayers   = 1,
            .imageUsage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode   = VK_SHARING_MODE_EXCLUSIVE,
            .preTransform       = w->surface_caps->currentTransform,
            .compositeAlpha     = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode        = w->present_mode,
            .clipped            = VK_TRUE,
            .oldSwapchain       = w->swapchain != VK_NULL_HANDLE ? w->swapchain : VK_NULL_HANDLE,
        };
        VkSwapchainKHR old_swapchain = w->swapchain;
        result = vkCreateSwapchainKHR(t->device, &swapchain_info, null, &w->swapchain);
        verify(result == VK_SUCCESS, "Failed to create swapchain");
        if (old_swapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(t->device, old_swapchain, null);
    }

    if (w->command_buffers) {
        for (int i = 0; i < w->image_count; i++) {
            if (w->image_available_semaphore) {
                vkDestroySemaphore(t->device, w->image_available_semaphore[i], null);
                vkDestroySemaphore(t->device, w->render_finished_semaphore[i], null);
            }
            vkDestroyFence(t->device, w->command_fences[i], null);
        }

        vkFreeCommandBuffers(t->device, t->command_pool, w->image_count, w->command_buffers);
        free(w->command_buffers);
        free(w->command_fences);
        if (w->image_available_semaphore) {
            free(w->image_available_semaphore);
            free(w->render_finished_semaphore);
        }
    }
    
    // Query new swapchain image count
    if (!w->backbuffer)
        vkGetSwapchainImagesKHR(t->device, w->swapchain, &w->image_count, null);
    else
        w->image_count = 1;

    /// create synchronization semaphores (decoupled from swap-chain)
    if (!w->backbuffer) {
        w->image_available_semaphore = calloc(w->image_count, sizeof(VkSemaphore));
        w->render_finished_semaphore = calloc(w->image_count, sizeof(VkSemaphore));
    }
    w->command_buffers = calloc(w->image_count, sizeof(VkCommandBuffer));
    w->command_fences  = calloc(w->image_count, sizeof(VkFence));

    VkCommandBufferAllocateInfo alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = t->command_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
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
    create_color_image  (w, &image_view_info);
    create_resolve_image(w, &image_view_info);

    // Get the swapchain images
    if (!w->backbuffer)
        vkGetSwapchainImagesKHR(t->device, w->swapchain, &w->image_count, swapchain_images);

    // Allocate new framebuffers
    w->framebuffers = malloc(w->image_count * sizeof(VkFramebuffer));
    for (uint32_t i = 0; i < w->image_count; ++i) {
        VkResult result;

        if (!w->backbuffer) {
            VkSemaphoreCreateInfo semaphore_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            result = vkCreateSemaphore(t->device, &semaphore_info, null, &w->image_available_semaphore[i]);
            verify(result == VK_SUCCESS, "failed to create image available semaphore");
            result = vkCreateSemaphore(t->device, &semaphore_info, null, &w->render_finished_semaphore[i]);
            verify(result == VK_SUCCESS, "failed to create render finished semaphore");
        }

        result = vkCreateFence(t->device, &(VkFenceCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT, // Initially signaled to allow the first use
            }, null, &w->command_fences[i]);
        verify(result == VK_SUCCESS, "failed to create fence");
        VkImageView swapchain_view = VK_NULL_HANDLE;

        if (!w->backbuffer) {
            // Create swapchain image view (this will be attachment [3])
            image_view_info.image = swapchain_images[i];
            result = vkCreateImageView(t->device, &image_view_info, null, &swapchain_view);
            verify(result == VK_SUCCESS, "Failed to create swapchain image view");
        }

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
            !w->backbuffer ? swapchain_view : w->resolve_view  // [2] - Resolve target (swapchain image; if its set)
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
    resize(w, width, height);
}

void model_init_pipeline(model m, Node n, Primitive prim, shader s) {
    Model      mdl          = m->id;
    trinity    t            = m->t;
    string     name         = prim->name ? prim->name : string("default");
    i64        indices      = prim->indices;
    Accessor   ai           = get(mdl->accessors, indices);
    i64        mat_id       = prim->material;
    AType      itype        = Accessor_member_type(ai);
    BufferView iview        = get(mdl->bufferViews, ai->bufferView);
    Buffer     ibuffer      = get(mdl->buffers,     iview->buffer);
    Material   material     = mdl->materials ? get(mdl->materials, mat_id) : null;
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

        members[member_count].type   = Accessor_member_type(ac);
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

    if (cmp(n->name, "Cube") == 0) {
        struct test_vertex {
            vec3f pos;
            vec2f uv;
        };

        struct test_vertex verts[14];
        i16 index[36];
        verify(index_size == 2, "invalid index");
        memcpy(verts, vdata, sizeof(struct test_vertex) * vertex_count);
        memcpy(index, idata, sizeof(i16) * index_count);
        int test2 = 2;
        test2 += 2;
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
        material,   material,
        samplers,   m->samplers);
    push(m->pipelines, pipe);
}

void model_init(model m) {
    Model mdl    = m->id;
    trinity t    = m->t;
    m->pipelines = array();
    if (!m->samplers) m->samplers = array();

    if (m->nodes) {
        each (m->nodes, node, n)
            each (n->parts, part, p)
                model_init_pipeline(m, n->id, p->id, p->s ? p->s : m->s);
    } else {
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
                    model_init_pipeline(m, n, prim, m->s);
            }
        }
    }
}

void PBR_init(PBR w) {
    mat4f_set_identity(&w->proj);
    mat4f_set_identity(&w->model);
    mat4f_set_identity(&w->view);
}

void Env_init(Env e) {
    mat4f_set_identity(&e->proj);
    mat4f_set_identity(&e->view);
}

void model_dealloc(model m) {
    /// pipelines should free automatically
}

image window_cast_image(window w) {
    trinity t = w->t;
    int     color_size = w->format == Pixel_rgbaf32 ? 16 : 4;
    int     image_size = w->width * w->height * color_size;
    buffer  b = buffer(t, t, size, image_size,
        u_dst, true, m_host_visible, true, m_host_coherent, true);
    transfer(b, w);
    void* data = mmap(b);
    image result = image(
        width,      w->width,
        height,     w->height,
        format,     w->format,
        surface,    Surface_color);
    memcpy(data(result), data, image_size);
    unmap(b);
    return result;
}

none command_init(command a) {
    trinity t = a->t;
    VkCommandBufferAllocateInfo alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = t->command_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(t->device, &alloc_info, &a->vk);
}

none command_begin(command a) {
    trinity t = a->t;
    vkBeginCommandBuffer(a->vk, &(VkCommandBufferBeginInfo){
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    });
}

none command_submit(command a) {
    trinity t = a->t;
    vkEndCommandBuffer(a->vk);
    VkSubmitInfo submit = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &a->vk
    };
    vkQueueSubmit(t->queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(t->queue);
}

none command_dealloc(command a) {
    trinity t = a->t;
    vkFreeCommandBuffers(t->device, t->command_pool, 1, &a->vk);
}

none create_mipmaps(trinity t, VkImage vk_image, int w, int h, int mips, int layers) {
    if (mips > 1) {
        int mipWidth  = w;
        int mipHeight = h;

        // transition mip level 0 to src optimal (needed for first blit)
        for (int f = 0; f < layers; f++)
            transition_image_layout(t, vk_image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1, f, 1);

        for (uint32_t i = 1; i < mips; i++) {
            for (int f = 0; f < layers; f++) {
                transition_image_layout(t, vk_image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, i, 1, f, 1);
                
                command cmd = command(t, t);
                begin(cmd);
                vkCmdBlitImage(cmd->vk,
                    vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &(VkImageBlit){
                        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, f, 1 },
                        .srcOffsets     = { {0, 0, 0}, {mipWidth, mipHeight, 1} },
                        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, f, 1 },
                        .dstOffsets     = { {0, 0, 0}, {max(1, mipWidth / 2), max(1, mipHeight / 2), 1} }
                    },
                    VK_FILTER_LINEAR);
                submit(cmd);
                drop(cmd);
                transition_image_layout(t, vk_image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, i, 1, f, 1);
            }

            mipWidth  = max(1, mipWidth / 2);
            mipHeight = max(1, mipHeight / 2);
        }
        for (int f = 0; f < layers; f++)
            transition_image_layout(t, vk_image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, mips, f, 1);
    } else {
        transition_image_layout(t, vk_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1, 0, 1);
    }
}

texture trinity_environment(trinity t, image img, vec3f rotation_axis, f32 rotation) {
    if (img->user) return (texture)img->user;

    int    size    = 256;
    const int mip_levels = 8;
    const int cube_count = mip_levels;
    window w       = window(t, t, format, Pixel_rgbaf32, backbuffer, true, width, size, height, size);
    image  clone   = image(
        source, data(img), width, img->width, height, img->height,
        format, Pixel_rgbaf32, surface, Surface_color);

    path   gltf     = path   ("models/env.gltf" );
    Model  data     = read   (gltf, typeid(Model) );
    Env    e        = Env    (t, t, name, string("env"));
    array  samplers = a      (clone, null);
    model  env      = model  (t, t, w, w, id, data, s, e, samplers, samplers);
    array  models   = a      (env, null);

    e->proj = mat4f_perspective (radians(90.0f), 1.0f, 0.1f, 10.0f);

    vec3f dirs[6] = {
        { 1,  0,  0 }, {-1,  0,  0 },
        { 0,  1,  0 }, { 0, -1,  0 },
        { 0,  0,  1 }, { 0,  0, -1 }};

    vec3f ups [6] = {
        { 0, -1,  0 }, { 0, -1,  0 },
        { 0,  0,  1 }, { 0,  0, -1 },
        { 0, -1,  0 }, { 0, -1,  0 }};
    vec3f eye = { 0, 0, 0 };

    
    VkImage vk_image[2]; /// cubes array that we return is of the number of mip levels;
    VkDeviceMemory vk_memory[2];

    VkImageCreateInfo image_info = {
        .sType          = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType      = VK_IMAGE_TYPE_2D,
        .format         = w->surface_format.format, // VK_FORMAT_R32G32B32A32_SFLOAT
        .extent         = { w->width, w->height, 1 },
        .mipLevels      = mip_levels, // usually log2(size) + 1
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .tiling         = VK_IMAGE_TILING_OPTIMAL,
        .usage          = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .flags          = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED
    };
    
    for (int i = 0; i < 2; i++) {
        image_info.arrayLayers = i == 0 ? 6 : 6 * cube_count;
        vkCreateImage(t->device, &image_info, NULL, &vk_image[i]);
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(t->device, vk_image[i], &memReqs);
        verify(vkAllocateMemory(t->device, &(VkMemoryAllocateInfo) {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = find_memory_type(t, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        }, null, &vk_memory[i]) == VK_SUCCESS, 
            "Failed to allocate memory for image");
        vkBindImageMemory(t->device, vk_image[i], vk_memory[i], 0);
    }

    command cmd = command(t, t);
    for (int f = 0; f < 6; f++) { 
        e->view = mat4f_look_at(&eye, &dirs[f], &ups[f]);
        process(w, models, null, null);
        transition_image_layout(t, w->resolve_image,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1, 0, 1);
        transition_image_layout(t, vk_image[0],
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, 1, f, 1);

        VkImageCopy region = {
            .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, f, 1 },
            .extent         = { size, size, 1 }
        };

        begin(cmd);
        vkCmdCopyImage(cmd->vk,
            w->resolve_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            vk_image[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region);
        submit(cmd);
    }

    create_mipmaps(t, vk_image[0], size, size, mip_levels, 6);

    texture cube = texture(
        t, t, vk_image, vk_image[0], surface, Surface_environment, vk_format, w->surface_format.format, mip_levels, mip_levels, layer_count, 6);

    /// convolve environment to create a multi-sampled cube
    Convolve conv_shader = Convolve (t, t, name, string("conv"), proj, e->proj);
    array  conv_samplers = a(cube, null); // must allow textures to register with gpu
    model  conv          = model  (t, t, w, w, id, data, s, conv_shader, samplers, conv_samplers);
    array  conv_models   = a(conv, null);

    vec4f v = vec4f(rotation_axis.x, rotation_axis.y, rotation_axis.z, radians(90.0f));
    quatf q = quatf(&v);
    conv_shader->env = mat4f_ident();
    conv_shader->env = mat4f_rotate(&conv_shader->env, &q);
    for (int a = 0; a < cube_count; a++) {
        float roughness = (float)a / (float)(mip_levels - 1);
        conv_shader->roughness_samples = vec2f(roughness, 1024.0f);
        
        for (int f = 0; f < 6; f++) {
            int face_id = a * 6 + f;
            conv_shader->view = mat4f_look_at(&eye, &dirs[f], &ups[f]);

            process(w, conv_models, null, null);
            //image screen = cast(image, w);
            //exr(screen, form(path, "screenshot.exr"));

            transition_image_layout(t, w->resolve_image,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1, 0, 1);
            transition_image_layout(t, vk_image[1],
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0, 1, face_id, 1);

            VkImageCopy region = {
                .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, face_id, 1 },
                .extent         = { size, size, 1 }
            };

            begin(cmd);
            vkCmdCopyImage(cmd->vk,
                w->resolve_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                vk_image[1], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &region);
            submit(cmd);
            for (uint32_t m = 1; m < mip_levels; m++) {
                int mip_size_src = size >> (m - 1);
                int mip_size_dst = size >> m;

                transition_image_layout(t, vk_image[1],
                    m == 1 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m - 1, 1, face_id, 1);

                transition_image_layout(t, vk_image[1],
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m, 1, face_id, 1);
                
                begin(cmd);
                vkCmdBlitImage(cmd->vk,
                    vk_image[1], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    vk_image[1], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &(VkImageBlit){
                        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, m - 1, face_id, 1 },
                        .srcOffsets     = { {0, 0, 0}, {mip_size_src, mip_size_src, 1} },
                        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, m, face_id, 1 },
                        .dstOffsets     = { {0, 0, 0}, {mip_size_dst, mip_size_dst, 1} }
                    },
                    VK_FILTER_LINEAR);
                submit(cmd);
            }
        }
    }
    
    for (int face_id = 0; face_id < 6 * cube_count; face_id++) {
        for (int m = 0; m < mip_levels; m++) {
            transition_image_layout(t, vk_image[1],
                (m == mip_levels - 1) ?
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : 
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                m, 1, face_id, 1);
        }
    }

    transition_image_layout(t, w->resolve_image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 1, 0, 1);
        
    texture convolve = texture(
        t, t,           vk_image, vk_image[1],
        surface,        Surface_environment,
        vk_format,      w->surface_format.format,
        mip_levels,     mip_levels,
        layer_count,    6 * cube_count);

    drop(cmd);
    drop(w);
    drop(cube);
    drop(clone);
    img->user = convolve;
    return img->user;
}

i64 A_virtual_size(AType t) {
    i64 type_size = t->size;
    if (!(t->traits & A_TRAIT_STRUCT || t->traits & A_TRAIT_PRIMITIVE))
        type_size -= sizeof(ARef);
    return type_size;
}

num uniform_size(AType type) {
    num   total = 0;
    for (int m = 0; m < type->member_count; m++) {
        struct type_member_t* mem = &type->members[m];
        bool inlay = mem->member_type == A_MEMBER_INLAY;
        if (mem->member_type == A_MEMBER_PROP || inlay)
            total += A_virtual_size(mem->type);
    }
    return total;
}



/// sub procedure of shader; transfer one type at a time
/// we may perform this in meta as well, but i think poly should be a base implementation
i64 uniform_transfer(shader instance, u8* data, AType type) {
    verify(instanceof(instance, shader), "shader instance not provided");
    num   index = 0;
    u8*   src   = instance;

    for (int m = 0; m < type->member_count; m++) {
        type_member_t* mem   = &type->members[m];
        bool inlay = mem->member_type == A_MEMBER_INLAY; // inlay is when you take a normal object and effectively have all of its structure memory in your own -- dealloc still in our A_dealloc
        if (mem->member_type == A_MEMBER_PROP || inlay) {
            int type_size = A_virtual_size(mem->type);
            if (inlay || mem->type->traits & A_TRAIT_STRUCT || mem->type->traits & A_TRAIT_PRIMITIVE)
                memcpy(&data[index], &src[mem->offset], type_size);
            else
                memcpy(&data[index], A_data((object) &src[mem->offset]), type_size);
            index += type_size;
        }
    }
    return index;
}


none gpu_sync(gpu a, window w) {
    trinity t = a->t;

    /// vbo/ibo only need maintenance upon init, less we want to transfer out
    if (a->vertex_size && !a->vertex) {
        bool comp = a->compute;
        a->vertex = buffer(t, t, size, a->vertex_size * a->vertex_count, 
            u_storage, comp, u_vertex, !comp, u_dst, !comp, u_shader, !comp,
            m_host_visible, true, m_host_coherent, true, data, a->vertex_data);
        
        if (a->index_size)
            a->index = buffer(
                t, t, size, a->index_size * a->index_count,
                u_index, true, u_dst, true, u_shader, true,
                m_host_visible, true, m_host_coherent, true, data, a->index_data);
    }

    if (a->sampler && !a->tx) {
        image img = instanceof(a->sampler, image);
        canvas cv = instanceof(a->sampler, canvas);
        texture tx = instanceof(a->sampler, texture);
        if (tx)
            a->tx = hold(tx);
        else if (img && img->user)
            a->tx = hold((texture)img->user);
        else if (img && img->surface == Surface_environment)
            a->tx = environment(t, img, (vec3f) { 0.0f, 1.0f, 0.0f }, radians(90.0f));
        else if (cv)
            a->tx = hold(cv->tx); // texture is created within canvas init
        else
            a->tx = texture(t, t, sampler, a->sampler);
    }
}

none gpu_init(gpu a) {
    if (a->vertex_size) a->vertex_data = A_alloc(typeid(i8), a->vertex_size * a->vertex_count, false);
    if (a->index_size)  a->index_data  = A_alloc(typeid(i8), a->index_size  * a->index_count,  false);
}

none gpu_dealloc(gpu a) {
    vkFreeMemory   (a->t->device, a->vk_memory, null);
}

define_class(gpu);


static VkFormat vk_format(Pixel f, bool linear) {
    VkFormat vk_format =
        f == Pixel_f32     ? VK_FORMAT_R32_SFLOAT : 
        f == Pixel_rgbaf32 ? VK_FORMAT_R32G32B32A32_SFLOAT : 
        f == Pixel_rgbf32  ? VK_FORMAT_R32G32B32_SFLOAT : 
        f == Pixel_rgba8   ? linear ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB : 
        f == Pixel_rgb8    ? linear ? VK_FORMAT_R8G8B8_UNORM   : VK_FORMAT_R8G8B8_SRGB   : 
        f == Pixel_u8      ? VK_FORMAT_R8_UNORM : 0;
    verify(vk_format, "incompatible image format: %o", e_str(Pixel, f));
    return vk_format;
}

static none placeholder_image(object sampler, int size, u8* fill, int* sampler_size, Pixel* format) {
    vector_i8    v_i8       = instanceof(sampler, vector_i8);     // grayscale i8
    vector_f32   v_f32      = instanceof(sampler, vector_f32);    // grayscale f32
    vector_rgb8  v_rgb8     = instanceof(sampler, vector_rgb8);   // rgb bytes
    vector_rgbf  v_rgbf     = instanceof(sampler, vector_rgbf);   // rgb floats
    vector_rgba8 v_rgba8    = instanceof(sampler, vector_rgba8);  // rgba bytes
    vector_rgbaf v_rgbaf    = instanceof(sampler, vector_rgbaf);  // rgba floats
    i8*          data_i8    = v_i8    ? data(v_i8)    : null;
    f32*         data_f32   = v_f32   ? data(v_f32)   : null;
    rgb8*        data_rgb8  = v_rgb8  ? data(v_rgb8)  : null;
    rgbf*        data_rgbf  = v_rgbf  ? data(v_rgbf)  : null;
    rgba8*       data_rgba8 = v_rgba8 ? data(v_rgba8) : null;
    rgbaf*       data_rgbaf = v_rgbaf ? data(v_rgbaf) : null;

    /// placeholder samplers, (4x4 is minimum we may create)
    int s = size * size;
    if (data_f32) {
        f32  *f32_fill = (f32*) fill;
        for (int i = 0; i < s; i++)
            f32_fill[i] = *data_f32;
        *format = Pixel_f32;
        *sampler_size = s * sizeof(f32);
    } else if (data_i8) {
        u8  *u8_fill = (u8*) fill;
        for (int i = 0; i < s; i++)
            u8_fill[i] = *data_i8;
        *format = Pixel_u8;
        *sampler_size = s * sizeof(u8);
    } else if (data_rgbf) {
        rgbf* v3_fill  = (rgbf*)fill;
        for (int i = 0; i < s; i++) v3_fill[i] = *data_rgbf;
        *format = Pixel_rgbf32;
        *sampler_size = s * sizeof(rgbf);
    } else if (data_rgb8) {
        rgb8* v3_fill  = (rgb8*)fill;
        for (int i = 0; i < s; i++) v3_fill[i] = *data_rgb8;
        *format = Pixel_rgb8;
        *sampler_size = s * sizeof(rgb8);
    } else if (data_rgbaf) {
        rgbaf* v4_fill  = (rgbaf*)fill;
        for (int i = 0; i < s; i++) v4_fill[i] = *data_rgbaf;
        *format = Pixel_rgbaf32;
        *sampler_size = s * sizeof(rgbaf);
    } else if (data_rgba8) {
        rgba8* v4_fill  = (rgba8*)fill;
        for (int i = 0; i < s; i++) v4_fill[i] = *data_rgba8;
        *format = Pixel_rgba8;
        *sampler_size = s * sizeof(rgba8);
    } else {
        fault("sampler data required");
    }
}

none texture_init(texture a) {
    trinity t         = a->t;
    image   img       = instanceof(a->sampler, image);
    int     ansio     = 4;
    i32     sampler_size = 0;
    u8      fill[256]; // 4 * 4 = 16 * 4 = 64 bytes max, so we use this as staging
    if (a->canvas)
        img = a->canvas;
    
    a->mip_levels  = a->mip_levels  ? a->mip_levels  : 1;
    a->layer_count = a->layer_count ? a->layer_count : 1;

    if (a->vk_image) {
        if (!a->vk_format)
            a->vk_format = vk_format(a->format, a->canvas != null);
        verify(a->vk_format, "expected format");

    } else {

        if (img) {
            a->format      = img->format;
            sampler_size   = a->canvas ? 0 : byte_count(img);
            ansio          = a->canvas ? 0 : 4;
            a->mip_levels  = a->canvas ? 1 : 8;
            a->width       = img->width;
            a->height      = img->height;
        } else {
            a->width     = 4;
            a->height    = 4;
            ansio        = 0;
            placeholder_image(a->sampler, 4, fill, &sampler_size, &a->format);
        }
        
        a->vk_format = vk_format(a->format, a->canvas != null);

        float* cubemap = null;
        verify(vkCreateImage(t->device, &(VkImageCreateInfo) {
            .sType          = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType      = VK_IMAGE_TYPE_2D,
            .format         = a->vk_format,
            .flags          = a->layer_count >= 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0,
            .extent         = { a->width, a->height, 1 },
            .mipLevels      = a->mip_levels,
            .arrayLayers    = a->layer_count,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .tiling         = VK_IMAGE_TILING_OPTIMAL,
            .usage          = (a->canvas ? 0 : VK_IMAGE_USAGE_TRANSFER_DST_BIT) | 
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .sharingMode    = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED
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
        
        for (int f = 0; f < a->layer_count; f++)
            transition_image_layout(t, a->vk_image,
                VK_IMAGE_LAYOUT_UNDEFINED,
                a->canvas ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : 
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0, 1, f, 1);

        if (!a->canvas) {
            command cmd = command(t, t);
            begin(cmd);
            buffer stagingBuffer = buffer(
                t, t, size, sampler_size, u_src, true, u_shader, true,
                m_host_visible, true, m_host_coherent, true,
                data, cubemap ? (object)cubemap : img ? data(img) : (object)fill);
            
            vkCmdCopyBufferToImage(cmd->vk, stagingBuffer->vk_buffer, a->vk_image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkBufferImageCopy) {
                .bufferOffset      = 0,
                .bufferRowLength   = 0,
                .bufferImageHeight = 0,
                .imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, a->layer_count },
                .imageOffset = {0, 0, 0},
                .imageExtent = { a->width, a->height, 1 }
            });
            submit(cmd);
            create_mipmaps(t, a->vk_image, a->width, a->height, a->mip_levels, a->layer_count);
        }
    }

    if (!a->vk_image_view) {
        VkImageViewCreateInfo viewInfo = {
            .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image      = a->vk_image,
            .viewType   = (a->layer_count >  6) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : 
                          (a->layer_count == 6) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D,
            .format     = a->vk_format,
            .subresourceRange = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = a->mip_levels,
                .baseArrayLayer = 0,
                .layerCount     = a->layer_count
            }
        };
        verify(vkCreateImageView(t->device, &viewInfo, NULL, &a->vk_image_view) == VK_SUCCESS, 
            "Failed to create VkImageView");
    }

    if (!a->vk_sampler) {
        VkSamplerAddressMode address_mode = (a->layer_count >= 6) ? 
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerCreateInfo samplerInfo = {
            .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter               = VK_FILTER_LINEAR,  // Smooth upscaling
            .minFilter               = VK_FILTER_LINEAR,  // Smooth downscaling
            .addressModeU            = address_mode,
            .addressModeV            = address_mode,
            .addressModeW            = address_mode,
            .anisotropyEnable        = (ansio > 0) ? VK_TRUE : VK_FALSE,
            .maxAnisotropy           = ansio,
            .minLod                  = 0.0f,
            .maxLod                  = (float)a->mip_levels,
            .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            .unnormalizedCoordinates = VK_FALSE,
            .compareEnable           = VK_FALSE,
            .compareOp               = VK_COMPARE_OP_ALWAYS,
            .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        };
        verify(vkCreateSampler(t->device, &samplerInfo, NULL, &a->vk_sampler) == VK_SUCCESS, 
            "Failed to create VkSampler");
    }
}

none texture_dealloc(texture a) {
    vkDestroySampler  (a->t->device, a->vk_sampler,    null);
    vkDestroyImageView(a->t->device, a->vk_image_view, null);
    vkDestroyImage    (a->t->device, a->vk_image,      null);
    vkFreeMemory      (a->t->device, a->vk_memory,     null);
}

define_class(texture)


VkIndexType gpu_index_type(gpu a) {
    return a->index_size == 1 ?
        VK_INDEX_TYPE_UINT8_KHR  :
        a->index_size == 2 ?
        VK_INDEX_TYPE_UINT16 :
        VK_INDEX_TYPE_UINT32;
}

static pbrMetallicRoughness pbr_defaults() {
    static pbrMetallicRoughness pbr;
    if (!pbr) {
        pbr = pbrMetallicRoughness();
        pbr->baseColorFactor            = vec4f(1, 1, 1, 1);
        pbr->metallicFactor             = 1.0f;
        pbr->roughnessFactor            = 1.0f;
        pbr->emissiveFactor             = vec3f(0, 0, 0);
        pbr->diffuseFactor              = vec4f(1, 1, 1, 1);
        pbr->specularFactor             = vec3f(1, 1, 1);
        pbr->glossinessFactor           = 1.0f;
        pbr->sheenColorFactor           = vec3f(0, 0, 0);
        pbr->sheenRoughnessFactor       = 0.0f;
        pbr->clearcoatFactor            = 0.0f;
        pbr->clearcoatRoughnessFactor   = 0.0f;
        pbr->transmissionFactor         = 0.0f;
        pbr->thicknessFactor            = 0.0f;
        pbr->attenuationColor           = vec3f(1, 1, 1);
        pbr->attenuationDistance        = 1e20f;
        pbr->ior                        = 1.5f;
        pbr->specularColorFactor        = vec3f(1, 1, 1);
        pbr->emissiveStrength           = 1.0f;
        pbr->iridescenceFactor          = 0.0f;
        pbr->iridescenceIor             = 1.3f;
        pbr->iridescenceThicknessMinimum= 100.0f;
        pbr->iridescenceThicknessMaximum= 400.0f;
    }
    return pbr;
}

gpu Surface_resource(Surface surface_value, pipeline p) {
    gpu res = null;
    trinity t = p->t;
    /// check if user provides an image
    each (p->samplers, image, img) {
        texture tx = instanceof((object)img, texture);
        if ((tx && tx->surface == surface_value) || (int)img->surface == surface_value) {
            res = gpu(t, t, name, cstring(e_str(Surface, surface_value)), sampler, img);
            break;
        }
    }

    /// create resource fragment based on the texture type
    pbrMetallicRoughness pbr_default = pbr_defaults();
    pbrMetallicRoughness pbr = p->material ? p->material->pbr : pbr_default;
    shader s = p->s;
 
    if (!res) {
        rgbaf f_normal = rgbaf(0.5f, 0.5f, 1.0f, 1.0f);
        rgbaf f_zero   = rgbaf(0.0f, 0.0f, 0.0f, 0.0f);
        rgbaf f_one    = rgbaf(1.0f, 1.0f, 1.0f, 1.0f);
        rgbaf f_color  = rgbaf(&pbr->baseColorFactor);
        shape single = shape_new(1, 0);
        switch (surface_value) {
            case Surface_normal:
                res = gpu(t, t, name, "normal", sampler,
                    vector_rgbaf_new(single, f_normal)); // Default normal map
                break;
            case Surface_metal:
                res = gpu(t, t, name, "metal", sampler,
                    vector_f32_new(single, pbr->metallicFactor)); // Default: Non-metallic
                break;
            case Surface_rough:
                res = gpu(t, t, name, "rough", sampler,
                    vector_f32_new(single, pbr->roughnessFactor)); // Default: Fully rough
                break;
            case Surface_emission:
                verify(pbr->emissiveStrength / 8.0f <= 1.0f, "refactor emissiveStrength");
                rgbaf f_emission = rgbaf(
                    pbr->emissiveFactor.x, pbr->emissiveFactor.y, pbr->emissiveFactor.z,
                    pbr->emissiveStrength / 8.0f);
                res = gpu(t, t, name, "emission", sampler,
                    vector_rgbaf_new(single, f_zero)); // Default: No emission
                break;
            case Surface_height:
                res = gpu(t, t, name, "height", sampler,
                    vector_f32_new(single, 0.5f)); // Midpoint height (0.5 = no change)
                break;
            case Surface_ao:
                res = gpu(t, t, name, "ao", sampler,
                    vector_f32_new(single, 1.0f)); // Full ambient occlusion by default
                break;
            case Surface_ior:
                res = gpu(t, t, name, "ior", sampler,
                    vector_f32_new(single, pbr->ior)); // IOR = 1.5 is blenders default
                break;
            case Surface_color:
                res = gpu(t, t, name, "color", sampler,
                    vector_rgbaf_new(single, f_color)); // White base color
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
    return res;
}

void pipeline_bind_resources(pipeline p) {
    trinity t     = p->t;
    int sampler_count = 0;
    int uniform_count = 0;
    VkDescriptorSetLayoutBinding sampler_bindings [32];
    VkDescriptorSetLayoutBinding uniform_bindings [32];
    VkDescriptorBufferInfo       buffer_infos     [32];
    VkDescriptorImageInfo        image_infos      [32];
    VkWriteDescriptorSet         descriptor_writes[32];

    p->resources = array(alloc, 32);
    if (p->vbo) {
        sync(p->vbo, p->w);
        push(p->resources, p->vbo);
    }
    
    VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;

    // populate bind layout for uniforms
    p->shader_uniforms = uniforms(t, t, s, p->s); // no need to have this in 'gpu' magic container

    each (p->shader_uniforms->u_buffers, buffer, b) {
        uniform_bindings[uniform_count] = (VkDescriptorSetLayoutBinding) {
            .binding         = uniform_count,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = stage_flags
        };
        buffer_infos[uniform_count]  = (VkDescriptorBufferInfo) {
            .buffer = b->vk_buffer,
            .offset = 0,
            .range = b->size
        };
        uniform_count++;
    }

    //uniform_count = 1; // test !
    
    // use enum type with value and meta type (context)
    AType shader_schema = isa(p->s);
    while (shader_schema != typeid(shader)) {
        for (int i = 0; i < shader_schema->member_count; i++) {
            type_member_t* mem = &shader_schema->members[i];

            // only look at attributes
            if (mem->member_type != A_MEMBER_ATTR)
                continue;

            // get enum type, and value
            AType  enum_type  = mem->type;
            i64    enum_value = mem->id;
            AType  meta_type  = mem->args.meta_0;
            verify(meta_type, "meta data not set on Surface/extension");

            type_member_t* fn = A_member(enum_type, A_MEMBER_SMETHOD, "resource", false);
            typedef object(*Resource)(i64, pipeline);
            Resource resf = fn ? fn->ptr : null;
            verify(resf, "unhandled gpu resource for type: %s", enum_type->name);
            gpu res = resf(enum_value, p);
            verify(instanceof(res, gpu), "expected gpu resource");

            /// add gpu resource; this is what will be updated when required
            sync(res, p->w);
            push(p->resources, res);

            sampler_bindings[sampler_count] = (VkDescriptorSetLayoutBinding) {
                .binding = sampler_count,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = stage_flags
            };
            image_infos[sampler_count] = (VkDescriptorImageInfo) {
                .sampler     = res->tx->vk_sampler,
                .imageView   = res->tx->vk_image_view, 
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            sampler_count++;
        }
        shader_schema = shader_schema->parent_type;
    }

    print("sampler count = %i", sampler_count);
    // at this point, somehow sampler count is 0? wtf. it went did ++ about 6 times above, as i traced it. seems to be a namespace issue but i see no other places for it
    vkCreateDescriptorSetLayout(p->t->device, &(VkDescriptorSetLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = uniform_count,
        .pBindings = uniform_bindings
    }, null, &p->descriptor_layouts[0]);

    vkCreateDescriptorSetLayout(p->t->device, &(VkDescriptorSetLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = sampler_count,
        .pBindings = sampler_bindings
    }, null, &p->descriptor_layouts[1]);

    VkDescriptorPoolSize sizes[2] = {};
    VkDescriptorPoolCreateInfo desc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 0,
        .pPoolSizes = sizes,
        .maxSets = 2
    };
    if (uniform_count) {
        sizes[desc.poolSizeCount].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sizes[desc.poolSizeCount++].descriptorCount = uniform_count;
    }
    if (sampler_count) {
        sizes[desc.poolSizeCount].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sizes[desc.poolSizeCount++].descriptorCount = sampler_count;
    }
    vkCreateDescriptorPool(p->t->device, &desc, null, &p->descriptor_pool);

    vkAllocateDescriptorSets(p->t->device, &(VkDescriptorSetAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = p->descriptor_pool,
        .descriptorSetCount = 2,
        .pSetLayouts = p->descriptor_layouts
    }, p->descriptor_sets);
    
    // Update descriptor set with uniform buffers
    for (int i = 0; i < uniform_count; i++) {
        descriptor_writes[i] = (VkWriteDescriptorSet) {
            .sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet             = p->descriptor_sets[0],
            .dstBinding         = i,
            .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount    = 1,
            .pBufferInfo        = &buffer_infos[i]
        };
    }
    // inadequate because we have lost the polymorphic relationship; we should instead iterate through the types here
    for (int i = 0; i < sampler_count; i++) {
        descriptor_writes[uniform_count + i] = (VkWriteDescriptorSet) {
            .sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet             = p->descriptor_sets[1],
            .dstBinding         = i,
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

    //if (p->memory) sync(p->memory);
    verify(vbo, "no vbo or memory provided to form a compute or graphical pipeline");
    i32     vertex_size  = vbo->vertex_size;
    i32     vertex_count = vbo->vertex_count;
    i32     index_size   = vbo->index_size;
    i32     index_count  = vbo->index_count;
    trinity t            = p->t;
    window  w            = p->w;

    Basic basic = p->s;
    mat4f* addr = &basic->proj;

    bind_resources(p);
    
    // Pipeline Layout (Bindings and Layouts)
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 2,
        .pSetLayouts            = p->descriptor_layouts,
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
                (mem->type == typeid(rgba8)) ? VK_FORMAT_R8G8B8A8_UNORM      : 
                (mem->type == typeid( f32 )) ? VK_FORMAT_R32_SFLOAT          :
                (mem->type == typeid( i8  )) ? VK_FORMAT_R8_SINT             :
                (mem->type == typeid( u8  )) ? VK_FORMAT_R8_UINT             :
                (mem->type == typeid( i16 )) ? VK_FORMAT_R16_SINT            :
                (mem->type == typeid( u16 )) ? VK_FORMAT_R16_UINT            :
              //(mem->type == typeid( i32 )) ? VK_FORMAT_R32_SINT            : -- glTF does not support
                (mem->type == typeid( u32 )) ? VK_FORMAT_R32_UINT            :
                                               VK_FORMAT_UNDEFINED;
            verify(attributes[attr_count].format != VK_FORMAT_UNDEFINED,
                "undefined vertex attribute for type %s", mem->type->name);
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
            .cullMode                = VK_CULL_MODE_NONE,
            .frontFace               = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable         = VK_FALSE,
        };

        VkPipelineMultisampleStateCreateInfo multisample_state = {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples  = t->msaa_samples,
            .sampleShadingEnable   = VK_TRUE,
        };

        VkPipelineColorBlendAttachmentState color_blend_attachment = {
            .blendEnable           = VK_TRUE,
            .colorWriteMask        = VK_COLOR_COMPONENT_R_BIT |
                                        VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT |
                                        VK_COLOR_COMPONENT_A_BIT,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp        = VK_BLEND_OP_ADD
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
        result = vkCreateComputePipelines(
            t->device, VK_NULL_HANDLE, 1, & (VkComputePipelineCreateInfo) {
            .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .layout = p->layout,
            .stage  = {
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = p->s->vk_comp,
                .pName  = "main",
            },
        }, null, &p->vk_compute);
        verify(result == VK_SUCCESS, "failed to create compute pipeline");
    }
}


void pipeline_dealloc(pipeline p) {
    trinity t = p->t;
    vkDestroyPipelineLayout(t->device, p->layout, null);
    if (p->vk_render)     vkDestroyPipeline(t->device, p->vk_render,     null);
    if (p->vk_compute)    vkDestroyPipeline(t->device, p->vk_compute,    null);
}

void pipeline_render(pipeline p, handle f) {
    VkCommandBuffer frame = f;

    /// for each uniform resource instanced from shader
    update(p->shader_uniforms);

    if (p->vk_compute) {
        vkCmdBindPipeline(frame, VK_PIPELINE_BIND_POINT_COMPUTE, p->vk_compute);
        vkCmdBindDescriptorSets(frame, VK_PIPELINE_BIND_POINT_COMPUTE, p->layout, 0, 1, &p->bind, 0, null);
        vkCmdDispatch(frame, p->memory->vertex_count, 1, 1);
    }
    if (p->vk_render) {
        vkCmdBindPipeline(frame, VK_PIPELINE_BIND_POINT_GRAPHICS, p->vk_render);
        VkDeviceSize offsets[] = { 0 };

        vkCmdBindDescriptorSets(frame, VK_PIPELINE_BIND_POINT_GRAPHICS, p->layout, 0, 2, p->descriptor_sets, 0, null);
        vkCmdBindVertexBuffers(frame, 0, 1, &p->vbo->vertex->vk_buffer, offsets);

        if (p->vbo->index) {
            vkCmdBindIndexBuffer(frame, p->vbo->index->vk_buffer, 0,
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
    if (!w->surface) {
        t->queue_family_index = 0; // works for testing
        found = true;
    } else
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

    VkResult result;

    // Initialize GLFW window with Vulkan compatibility
    if (!w->backbuffer) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        w->window = glfwCreateWindow(w->width, w->height,
            w->title ? cstring(w->title) : "trinity", null, null);
        verify(w->window, "Failed to create GLFW window");
        glfwSetWindowUserPointer(w->window, (void *)w);
        glfwSetKeyCallback(w->window, handle_glfw_key);
        glfwSetFramebufferSizeCallback(w->window, handle_glfw_framebuffer_size);

        // Create Vulkan surface
        result = glfwCreateWindowSurface(t->instance, w->window, null, &w->surface);
        verify(result == VK_SUCCESS, "Failed to create Vulkan surface");
    }

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


    if (!w->backbuffer) {
        // Query surface capabilities
        w->surface_caps = calloc(1, sizeof(VkSurfaceCapabilitiesKHR));
        result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(t->physical_device, w->surface, w->surface_caps);
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
            print("format = %i, color space = %i", formats[i].format, formats[i].colorSpace);
            if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
                VkSurfaceFormatKHR* f = &formats[i];
                w->surface_format = formats[i];
                //break;
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
        w->extent.width              = (uint32_t)width;
        w->extent.height             = (uint32_t)height;
    } else {
        w->surface_format.format     = w->format == Pixel_rgbaf32 ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
        w->surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        w->extent.width              = w->width;
        w->extent.height             = w->height;
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
            .finalLayout    = !w->backbuffer ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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
    w->semaphore_frame = 0;
    w->last_fence = -1;
    resize(w, w->extent.width, w->extent.height);  // Call the framebuffer update directly
}

void window_process(window w, array models, object callback, object arg) {
    trinity  t = w->t;
    void(*user_fn)(pipeline, ARef) = callback;
    uint32_t index = 0;
    VkResult result;

    if (!w->backbuffer) {
        result = vkAcquireNextImageKHR(
            t->device, w->swapchain, UINT64_MAX,
            w->image_available_semaphore[w->semaphore_frame], VK_NULL_HANDLE, &index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            handle_glfw_framebuffer_size(w->window, w->width, w->height);
            return
            ;
        }
        verify(result == VK_SUCCESS, "failed to acquire swapchain image");
    }

    vkResetFences  (t->device, 1, &w->command_fences[index]);
    w->last_fence = index;
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
            { .color        = { .float32 = { 0.044f, 0.044f, 0.044f, 1.0f }} },
            { .depthStencil = { .depth   =   1.0f,
                                .stencil =   0 } },
            { .color        = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} } }
        }
    }, VK_SUBPASS_CONTENTS_INLINE);

    each(models, model, m) {
        each(m->pipelines, pipeline, p) {
            if (instanceof(p->s, PBR))
                ((PBR)p->s)->model = p->model;
        
            if (callback) // user will always store their own references to their own shader
                ((u_callback)callback)(p, arg);
                
            // important that this happen instantly after user sets
            update(p->shader_uniforms);
            // user may indeed set uniforms different depending on the pipeline; this is quite natural and not always redundant
            render(p, frame);
        }
    }
    vkCmdEndRenderPass(frame);   // End render pass
    vkEndCommandBuffer(frame);   // End command buffer recording

    // Submit command buffer to the queue
    VkPipelineStageFlags s_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo     = {
        .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount     = 1,
        .pCommandBuffers        = &frame,
        .waitSemaphoreCount     = !w->backbuffer ? 1                                                 : 0,
        .pWaitSemaphores        = !w->backbuffer ? &w->image_available_semaphore[w->semaphore_frame] : null,
        .pWaitDstStageMask      = !w->backbuffer ? &s_flags                                          : null,
        .signalSemaphoreCount   = !w->backbuffer ? 1                                                 : 0,
        .pSignalSemaphores      = !w->backbuffer ? &w->render_finished_semaphore[w->semaphore_frame] : null
    };

    vkQueueSubmit(t->queue, 1, &submitInfo, w->command_fences[index]);

    if (!w->backbuffer) {
        // Present the swapchain image
        VkPresentInfoKHR presentInfo = {
            .sType                  = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .swapchainCount         = 1,
            .pSwapchains            = &w->swapchain,
            .pImageIndices          = &index,
            .waitSemaphoreCount     = 1,
            .pWaitSemaphores        = &w->render_finished_semaphore[w->semaphore_frame]
        };

        result = vkQueuePresentKHR(t->queue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            w->semaphore_frame = (w->semaphore_frame + 1) % w->image_count;
            return;
        }
        verify(result == VK_SUCCESS, "present");
    }
    
    result = vkWaitForFences(t->device, 1, &w->command_fences[index], VK_TRUE, UINT64_MAX);
    verify(result == VK_SUCCESS, "fence wait failed: last_fence=%d", w->last_fence);

    if (!w->backbuffer)
        w->semaphore_frame = (w->semaphore_frame + 1) % w->image_count;
}

int window_loop(window w, ARef callback, ARef arg) {
    trinity t = w->t;
    void(*user_fn)(pipeline, ARef) = callback;
    while (!glfwWindowShouldClose(w->window)) {
        glfwPollEvents();
        process(w, w->models, user_fn, arg);
    }
    return 0;
}

void window_dealloc(window w) {
    trinity t = w->t;
    
    // Clean up synchronization objects
    for (int i = 0; i < w->image_count; i++) {
        if (w->image_available_semaphore) {
            vkDestroySemaphore(t->device, w->image_available_semaphore[i], null);
            vkDestroySemaphore(t->device, w->render_finished_semaphore[i], null);
        }
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
            if (w->depth_images[i])
                vkDestroyImage(t->device, w->depth_images[i], null);
        }
        free(w->depth_images);
    }
    
    free(w->command_buffers);
    free(w->command_fences);
    vkDestroyRenderPass(t->device, w->render_pass, null);

    if (!w->backbuffer) {
        free(w->image_available_semaphore);
        free(w->render_finished_semaphore);
        vkDestroySwapchainKHR(t->device, w->swapchain, null);
        vkDestroySurfaceKHR(t->instance, w->surface, null);
        glfwDestroyWindow(w->window);
    }
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
}

/// instance uniforms data for a shader with uniforms
void uniforms_update(uniforms a) {
    each(a->u_buffers, buffer, b) {
        uniform_transfer(a->s, b->data, (AType)b->user);
        update(b, null);
    }
}

void uniforms_init(uniforms a) {
    trinity t = a->t;
    list types = list();
    Basic basic = a->s;
    print("from uniforms_init: basic = %p, &basic->proj = %p", basic, &basic->proj); 

    AType ty = isa(a->s);
    while (ty != typeid(shader)) {
        insert_after(types, ty, -1); // todo: would be nice if AType can be an object -- we gave it static A header above each, so it should
        ty = ty->parent_type;
    }
    int    u_index  = 0;
    i32    total_uniform = 0;

    print("types for %s", isa(a->s)->name);
    for (item i = types->first; i; i = i->next) {
        AType ty       = i->value;
        i32   u_size   = uniform_size(ty);
        total_uniform += u_size;
    }
    
    a->u_buffers = array(4);
    a->u_memory = A_alloc(typeid(u8), total_uniform, false);
    u8*     src = a->u_memory;
    for (item i = types->first; i; i = i->next) {
        AType ty = i->value;
        num u_size = uniform_size(ty);
        buffer uniform = buffer(t, t, size, u_size,
            u_uniform, true, u_dst, true, m_host_visible, true, m_host_coherent, true,
            data, &src[u_index], user, (ARef)ty);
        push(a->u_buffers, uniform);
        u_index += u_size;
    }
}

void shader_init(shader s) {
    trinity t = s->t;
    if (!t) return; // for mock data and testing

    // generate .spv for shader resources
    string spv_file;
    bool found = false;
    
    s->frag = form(string, "shaders/%o.frag", s->name);
    s->vert = form(string, "shaders/%o.vert", s->name);
    s->comp = form(string, "shaders/%o.comp", s->name);

    if (!s->name) {
        AType type = isa(s);
        verify(type != typeid(shader), "base shader usage requires a shader name");
        s->name = string(type->name);
    }
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

void shader_dealloc(shader s) {
    trinity t = s->t;
    vkDestroyShaderModule(t->device, s->vk_vert, null);
    vkDestroyShaderModule(t->device, s->vk_frag, null);
    vkDestroyShaderModule(t->device, s->vk_comp, null);
}


void trinity_init(trinity t) {
    verify(glfwInit(), "glfw init");
    int wsize2 = sizeof(struct _window);
    int bsize2 = sizeof(struct _Basic);
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
    t->queue_family_index = -1;
}

void trinity_dealloc(trinity t) {
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

void buffer_transfer(buffer b, window w) {
    VkImage image = w->resolve_image;
    trinity t = b->t;
    VkBuffer buffer = b->vk_buffer;

    VkCommandBufferAllocateInfo alloc_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = t->command_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(t->device, &alloc_info, &cmd);

    vkBeginCommandBuffer(cmd, &(VkCommandBufferBeginInfo){
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    });

    VkImageSubresourceRange subres_range = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
    };

    // Transition image to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .image               = image,
        .subresourceRange    = subres_range
    };
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, 0, 0, 0, 1, &barrier);

    VkBufferImageCopy region = {
        .bufferOffset      = 0,
        .bufferRowLength   = 0, // tightly packed
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel       = 0,
            .baseArrayLayer = 0,
            .layerCount     = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {w->width, w->height, 1}
    };

    vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    vkQueueSubmit(t->queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(t->queue);

    vkFreeCommandBuffers(t->device, t->command_pool, 1, &cmd);
}


none buffer_update(buffer a, ARef data) {
    if (!a->size) return;
    if (!data) data = (ARef)a->data;
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

none buffer_dealloc(buffer a) {
    vkDestroyBuffer(a->t->device, a->vk_buffer, null);
    vkFreeMemory   (a->t->device, a->vk_memory, null);
}

define_enum(Pixel)
define_enum(Filter)
define_enum(Polygon)
define_enum(Asset)
define_enum(Sampling)

define_class(trinity)
define_class(shader)
define_class(pipeline)
define_class(part)
define_class(node)
define_class(model)
define_class(window)
define_class(buffer)
define_class(command)
define_class(uniforms) 
define_class(IBL) 

// abstract identifier to indicate functionality 
// of non-texture case of attribute, still under the enumerable Surface

define_class(particle)

define_enum(Surface)
 
define_mod(PBR,   shader)
define_mod(Env,   shader)
define_mod(Convolve,  shader)

define_mod(Basic, shader)
