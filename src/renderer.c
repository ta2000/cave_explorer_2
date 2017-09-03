#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/vector3.h>

#include "linmath.h"

#include "renderer.h"

#define APP_NAME "Game"
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0
#define APP_VERSION_PATCH 0

#define VALIDATION_ENABLED 1

void renderer_create_resources(
        struct renderer_resources* resources,
        GLFWwindow* window)
{
    resources->instance = renderer_get_instance();
    assert(resources->instance != VK_NULL_HANDLE);

    PFN_vkCreateDebugReportCallbackEXT fp_create_debug_callback;
    fp_create_debug_callback =
            (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
                    resources->instance,
                    "vkCreateDebugReportCallbackEXT"
            );
    assert(*fp_create_debug_callback);

    resources->debug_callback_ext = renderer_get_debug_callback(
        resources->instance,
        fp_create_debug_callback
    );

    resources->surface = renderer_get_surface(
        resources->instance,
        window
    );
    assert(resources->surface != VK_NULL_HANDLE);

    const char* device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    uint32_t device_extension_count = 1;

    resources->physical_device = renderer_get_physical_device(
        resources->instance,
        resources->surface,
        device_extension_count,
        device_extensions
    );
    assert(resources->physical_device != VK_NULL_HANDLE);

	VkPhysicalDeviceFeatures required_features;
    memset(&required_features, VK_FALSE, sizeof(required_features));
    /*required_features.geometryShader = VK_TRUE;
    required_features.tessellationShader = VK_TRUE;
    required_features.fullDrawIndexUint32 = VK_TRUE;
    required_features.imageCubeArray = VK_TRUE;
    required_features.sampleRateShading = VK_TRUE;
    required_features.samplerAnisotropy = VK_TRUE;*/

    resources->device = renderer_get_device(
        resources->physical_device,
        resources->surface,
        &required_features,
        device_extension_count,
        device_extensions
    );
    assert(resources->device != VK_NULL_HANDLE);

    uint32_t graphics_family_index = renderer_get_graphics_queue(
        resources->physical_device
    );
    vkGetDeviceQueue(
        resources->device,
        graphics_family_index,
        0,
        &resources->graphics_queue
    );

    uint32_t present_family_index = renderer_get_present_queue(
        resources->physical_device,
        resources->surface
    );
    vkGetDeviceQueue(
        resources->device,
        present_family_index,
        0,
        &resources->present_queue
    );

    VkSurfaceFormatKHR image_format;
    image_format = renderer_get_image_format(
        resources->physical_device,
        resources->surface
    );

    int window_width, window_height;
    glfwGetWindowSize(window, &window_width, &window_height);
    resources->swapchain_extent = renderer_get_swapchain_extent(
        resources->physical_device,
        resources->surface,
        window_width,
        window_height
    );

    resources->swapchain = renderer_get_swapchain(
        resources->physical_device,
        resources->device,
        resources->surface,
        image_format,
        resources->swapchain_extent,
        VK_NULL_HANDLE
    );
    assert(resources->swapchain != VK_NULL_HANDLE);

    resources->command_pool = renderer_get_command_pool(
        resources->physical_device,
        resources->device
    );
    assert(resources->command_pool != VK_NULL_HANDLE);

    resources->swapchain_image_count = renderer_get_swapchain_image_count(
        resources->device,
        resources->swapchain
    );

    resources->swapchain_buffers = malloc(
        resources->swapchain_image_count * sizeof(*resources->swapchain_buffers)
    );

    renderer_create_swapchain_buffers(
        resources->device,
        resources->command_pool,
        resources->swapchain,
        image_format,
        resources->swapchain_buffers,
        resources->swapchain_image_count
    );

    VkFormat depth_format;
    depth_format = renderer_get_depth_format(
        resources->physical_device,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
    assert(depth_format != VK_FORMAT_UNDEFINED);

    resources->depth_image = renderer_get_depth_image(
        resources->physical_device,
        resources->device,
        resources->graphics_queue,
        resources->command_pool,
        resources->swapchain_extent,
        depth_format
    );

    resources->render_pass = renderer_get_render_pass(
        resources->device,
        image_format.format,
        depth_format
    );
    assert(resources->render_pass != VK_NULL_HANDLE);

    resources->framebuffers = malloc(
        resources->swapchain_image_count * sizeof(*resources->framebuffers)
    );
    assert(resources->framebuffers);
    renderer_create_framebuffers(
        resources->device,
        resources->render_pass,
        resources->swapchain_extent,
        resources->swapchain_buffers,
        resources->depth_image.image_view,
        resources->framebuffers,
        resources->swapchain_image_count
    );

    resources->descriptor_pool = renderer_get_descriptor_pool(
        resources->device
    );
    assert(resources->descriptor_pool != VK_NULL_HANDLE);

    resources->descriptor_layout = renderer_get_descriptor_layout(
        resources->device
    );
    assert(resources->descriptor_layout != VK_NULL_HANDLE);

    resources->uniform_buffer = renderer_get_uniform_buffer(
        resources->physical_device,
        resources->device,
        &resources->staging_uniform_buffer
    );

    resources->base_graphics_pipeline_layout = renderer_get_pipeline_layout(
        resources->device,
        &resources->descriptor_layout,
        1,
        NULL,
        0
    );

    resources->base_graphics_pipeline = renderer_get_base_graphics_pipeline(
        resources->device,
        resources->swapchain_extent,
        resources->base_graphics_pipeline_layout,
        resources->render_pass,
        0
    );

    renderer_load_textured_model(resources);

    renderer_record_draw_commands(
        resources->base_graphics_pipeline,
        resources->base_graphics_pipeline_layout,
        resources->render_pass,
        resources->swapchain_extent,
        resources->framebuffers,
        resources->swapchain_buffers,
        resources->swapchain_image_count,
        &resources->mesh
    );

    resources->image_available = renderer_get_semaphore(resources->device);
    resources->render_finished = renderer_get_semaphore(resources->device);
}

void renderer_render(
        struct renderer_resources* resources)
{
    renderer_update_uniform_buffer(
        resources->physical_device,
        resources->device,
        resources->graphics_queue,
        resources->command_pool,
        resources->swapchain_extent,
        &resources->uniform_buffer,
        &resources->staging_uniform_buffer
    );

    uint32_t image_index;
    VkResult result;
    result = vkAcquireNextImageKHR(
        resources->device,
        resources->swapchain,
        UINT64_MAX,
        resources->image_available,
        VK_NULL_HANDLE,
        &image_index
    );
    assert(result == VK_SUCCESS);

    VkSemaphore wait_semaphores[] = {resources->image_available};
    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkSemaphore signal_semaphores[] = {resources->render_finished};

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &(resources->swapchain_buffers[image_index].cmd),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores
    };

    result = vkQueueSubmit(
        resources->graphics_queue,
        1,
        &submit_info,
        VK_NULL_HANDLE
    );
    assert(result == VK_SUCCESS);

    VkSwapchainKHR swapchains[] = {resources->swapchain};

	VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &image_index,
        .pResults = NULL
    };

    vkQueuePresentKHR(resources->present_queue, &present_info);
}

void renderer_destroy_resources(
        struct renderer_resources* resources)
{
    vkDeviceWaitIdle(resources->device);

    uint32_t i;

    vkDestroySemaphore(resources->device, resources->image_available, NULL);
    vkDestroySemaphore(resources->device, resources->render_finished, NULL);

    vkDestroyBuffer(resources->device, resources->mesh.vbo.buffer, NULL);
    vkFreeMemory(resources->device, resources->mesh.vbo.memory, NULL);
    vkDestroyBuffer(resources->device, resources->mesh.ibo.buffer, NULL);
    vkFreeMemory(resources->device, resources->mesh.ibo.memory, NULL);

    vkDestroyImage(resources->device, resources->mesh.texture->image, NULL);
    vkDestroyImageView(
            resources->device, resources->mesh.texture->image_view, NULL);
    vkFreeMemory(resources->device, resources->mesh.texture->memory, NULL);
    vkDestroySampler(
            resources->device, resources->mesh.texture->sampler, NULL);
    free(resources->mesh.texture);

    vkDestroyPipelineLayout(
            resources->device, resources->base_graphics_pipeline_layout, NULL);
    vkDestroyPipeline(
            resources->device, resources->base_graphics_pipeline, NULL);

    vkDestroyBuffer(
            resources->device, resources->staging_uniform_buffer.buffer, NULL);
    vkFreeMemory(
            resources->device, resources->staging_uniform_buffer.memory, NULL);
    vkDestroyBuffer(resources->device, resources->uniform_buffer.buffer, NULL);
    vkFreeMemory(resources->device, resources->uniform_buffer.memory, NULL);

    vkDestroyDescriptorSetLayout(
            resources->device, resources->descriptor_layout, NULL);
    vkDestroyDescriptorPool(
            resources->device, resources->descriptor_pool, NULL);

    for (i=0; i<resources->swapchain_image_count; i++) {
        vkDestroyFramebuffer(
            resources->device,
            resources->framebuffers[i],
            NULL
        );
    }

    vkDestroyRenderPass(resources->device, resources->render_pass, NULL);

    vkDestroyImage(resources->device, resources->depth_image.image, NULL);
    vkDestroyImageView(
            resources->device, resources->depth_image.image_view, NULL);
    vkFreeMemory(resources->device, resources->depth_image.memory, NULL);

    for (i=0; i<resources->swapchain_image_count; i++) {
        vkDestroyImageView(
            resources->device,
            resources->swapchain_buffers[i].image_view,
            NULL
        );
        vkFreeCommandBuffers(
            resources->device,
            resources->command_pool,
            1,
            &resources->swapchain_buffers[i].cmd
        );
    }
    free(resources->swapchain_buffers);
    resources->swapchain_buffers = NULL;

    vkDestroyCommandPool(resources->device, resources->command_pool, NULL);

    vkDestroySwapchainKHR(
        resources->device,
        resources->swapchain,
        NULL
    );

    vkDestroyDevice(resources->device, NULL);

    vkDestroySurfaceKHR(
        resources->instance,
        resources->surface,
        NULL
    );

    PFN_vkDestroyDebugReportCallbackEXT fp_destroy_debug_callback;
    fp_destroy_debug_callback =
            (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
                    resources->instance,
                    "vkDestroyDebugReportCallbackEXT"
            );
    assert(*fp_destroy_debug_callback);
    fp_destroy_debug_callback(
        resources->instance,
        resources->debug_callback_ext,
        NULL
    );

    vkDestroyInstance(resources->instance, NULL);
}

VkInstance renderer_get_instance()
{
    VkInstance instance_handle;
    instance_handle = VK_NULL_HANDLE;

	// Create info
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0
    };

    // Application info
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = APP_NAME,
        .applicationVersion = VK_MAKE_VERSION(
                APP_VERSION_MAJOR,
                APP_VERSION_MINOR,
                APP_VERSION_PATCH),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(0,0,0),
        .apiVersion = VK_API_VERSION_1_0
    };
    create_info.pApplicationInfo = &app_info;

    // Validation layers
    const char* enabled_layers[] = {"VK_LAYER_LUNARG_standard_validation"};
    uint32_t enabled_layer_count = 1;

	VkLayerProperties* available_layers;
    uint32_t available_layer_count;

    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);
    assert(available_layer_count > 0);

    available_layers = malloc(
        available_layer_count * sizeof(*available_layers)
    );
    assert(available_layers);

    vkEnumerateInstanceLayerProperties(
        &available_layer_count,
        available_layers
    );

    uint32_t i, j;
    bool layer_found = true;
    for (i = 0; i < enabled_layer_count; i++)
    {
        if (VALIDATION_ENABLED)
            break;

        layer_found = false;
        for (j = 0; j < available_layer_count; j++)
        {
            if (!strcmp(enabled_layers[i], available_layers[j].layerName))
            {
                layer_found = true;
                break;
            }
        }

        if (layer_found)
            break;
    }
    assert(layer_found);

    if (VALIDATION_ENABLED) {
        create_info.enabledLayerCount = enabled_layer_count;
        create_info.ppEnabledLayerNames = enabled_layers;
    } else {
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = NULL;
    }

    // Extensions
    const char** glfw_extensions;
    uint32_t glfw_extension_count;
    glfw_extensions = glfwGetRequiredInstanceExtensions(
        &glfw_extension_count
    );

    const char* my_extensions[] = {VK_EXT_DEBUG_REPORT_EXTENSION_NAME};
    uint32_t my_extension_count = 1;

    char** all_extensions;
    uint32_t all_extension_count = glfw_extension_count + my_extension_count;
    all_extensions = malloc(all_extension_count * sizeof(char*));

    for (i=0; i<glfw_extension_count; i++)
    {
        all_extensions[i] = calloc(1, strlen(glfw_extensions[i]) + 1);
        strcpy(all_extensions[i], glfw_extensions[i]);
    }
    for (j=0; j<my_extension_count; j++)
    {
        all_extensions[i+j] = calloc(1, strlen(my_extensions[j]) + 1);
        strcpy(all_extensions[i+j], my_extensions[j]);
    }

    create_info.enabledExtensionCount = all_extension_count;
    create_info.ppEnabledExtensionNames = (const char* const*)all_extensions;

    // Create instance
    VkResult result;
    result = vkCreateInstance(
        &create_info,
        NULL,
        &instance_handle
    );
    assert(result == VK_SUCCESS);

    for (i=0; i<all_extension_count; i++)
        free(all_extensions[i]);
    free(all_extensions);
    free(available_layers);

    return instance_handle;
}

VkDebugReportCallbackEXT renderer_get_debug_callback(
        VkInstance instance,
        PFN_vkCreateDebugReportCallbackEXT fp_create_debug_callback)
{
    VkDebugReportCallbackEXT debug_callback_handle;

    VkDebugReportCallbackCreateInfoEXT debug_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                VK_DEBUG_REPORT_WARNING_BIT_EXT,
        .pUserData = NULL,
        .pfnCallback = &renderer_debug_callback,
    };

    VkResult result;
    result = fp_create_debug_callback(
                instance,
                &debug_info,
                NULL,
                &debug_callback_handle
            );
    assert(result == VK_SUCCESS);

    return debug_callback_handle;
}

VKAPI_ATTR VkBool32 VKAPI_CALL renderer_debug_callback(
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT obj_type,
        uint64_t obj,
        size_t location,
        int32_t code,
        const char* p_layer_prefix,
        const char* p_msg,
        void* p_user_data)
{
    // Alloc extra chars for additional info
    char* message = malloc(strlen(p_msg) + 256);
    assert(message);

    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        sprintf(message, "%s error, code %d: %s",
                p_layer_prefix, code, p_msg);
    }
    else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        sprintf(message, "%s warning, code %d: %s",
                p_layer_prefix, code, p_msg);
    }
    else
    {
        free(message);
        return VK_FALSE;
    }

    fprintf(stderr, "%s\n\n", message);

    free(message);

    return VK_FALSE;
}

VkSurfaceKHR renderer_get_surface(
        VkInstance instance,
        GLFWwindow* window)
{
    VkSurfaceKHR surface_handle;

    VkResult result;
    result = glfwCreateWindowSurface(
        instance,
        window,
        NULL,
        &surface_handle
    );
    assert(result == VK_SUCCESS);

    return surface_handle;
}

VkPhysicalDevice renderer_get_physical_device(
        VkInstance instance,
        VkSurfaceKHR surface,
        uint32_t device_extension_count,
        const char** device_extensions)
{
    VkPhysicalDevice physical_device_handle;
    physical_device_handle = VK_NULL_HANDLE;

    // Enumerate devices
    VkPhysicalDevice* physical_devices;
    uint32_t physical_device_count;

    vkEnumeratePhysicalDevices(instance, &physical_device_count, NULL);
    assert(physical_device_count > 0);

    physical_devices = malloc(
        physical_device_count * sizeof(*physical_devices)
    );
    assert(physical_devices);

    vkEnumeratePhysicalDevices(
        instance,
        &physical_device_count,
        physical_devices
    );

    uint32_t i;
    for (i=0; i<physical_device_count; i++)
    {
        // Ensure required extensions are supported
        if (physical_device_extensions_supported(
                physical_devices[i],
                device_extension_count,
                device_extensions))
        {
            printf("Extensions not supported\n");
            continue;
        }

        // Ensure there is at least one surface format
        // compatible with the surface
        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_devices[i],
            surface,
            &format_count,
            NULL
        );
        if (format_count < 1) {
            printf("No surface formats available\n");
            continue;
        }

        // Ensured there is at least one present mode available
        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_devices[i],
            surface,
            &present_mode_count,
            NULL
        );
        if (present_mode_count < 1) {
            printf("No present modes available for surface\n");
            continue;
        }

        // Ensure the physical device has WSI
        VkQueueFamilyProperties* queue_family_properties;
        uint32_t queue_family_count;

        vkGetPhysicalDeviceQueueFamilyProperties(
            physical_devices[i],
            &queue_family_count,
            NULL
        );

        queue_family_properties = malloc(
            queue_family_count * sizeof(*queue_family_properties)
        );

        vkGetPhysicalDeviceQueueFamilyProperties(
            physical_devices[i],
            &queue_family_count,
            queue_family_properties
        );

        uint32_t j;
        VkBool32 wsi_support = VK_FALSE;
        VkBool32 graphics_bit = VK_FALSE;
        for (j=0; j<queue_family_count; j++)
        {
            graphics_bit =
                queue_family_properties[j].queueCount > 0 &&
                queue_family_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT;

            VkResult wsi_query_result;
            wsi_query_result = vkGetPhysicalDeviceSurfaceSupportKHR(
                physical_devices[i],
                j,
                surface,
                &wsi_support
            );
            assert(wsi_query_result == VK_SUCCESS);

            if (wsi_support && graphics_bit)
                break;
        }
        if (!(wsi_support && graphics_bit)) {
            printf("Suitable GPU not found:\n");
            printf("Window System Integration: %d\n", wsi_support);
            printf("Graphical operations supported: %d\n", graphics_bit);
            continue;
        }

        free(queue_family_properties);

        physical_device_handle = physical_devices[i];
        break;
    }

    return physical_device_handle;
}

bool physical_device_extensions_supported(
    VkPhysicalDevice physical_device,
    uint32_t required_extension_count,
    const char** required_extensions)
{
    // Enumerate extensions for each device
    VkExtensionProperties* available_extensions;
    uint32_t available_extension_count;

    vkEnumerateDeviceExtensionProperties(
        physical_device,
        NULL,
        &available_extension_count,
        NULL
    );
    assert(available_extension_count > 0);

    available_extensions = malloc(
        available_extension_count * sizeof(*available_extensions)
    );
    assert(available_extensions);

    vkEnumerateDeviceExtensionProperties(
        physical_device,
        NULL,
        &available_extension_count,
        available_extensions
    );

    // Determine if device's extensions contain necessary extensions
    uint32_t i, j;
    for (i=0; i<required_extension_count; i++)
    {
        for (j=0; j<available_extension_count; j++)
        {
            if (strcmp(
                    required_extensions[i],
                    available_extensions[j].extensionName
                ))
            {
                return false;
            }
        }
    }

    return true;
}

uint32_t renderer_get_graphics_queue(
        VkPhysicalDevice physical_device)
{
    uint32_t graphics_queue_index;

    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        NULL
    );

    VkQueueFamilyProperties* queue_family_properties;
    queue_family_properties = malloc(
        queue_family_count * sizeof(*queue_family_properties)
    );

    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        queue_family_properties
    );

    uint32_t i;
    bool graphics_queue_found = false;
    for (i=0; i<queue_family_count; i++)
    {
        if (queue_family_properties[i].queueCount > 0 &&
            queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            graphics_queue_index = i;
            graphics_queue_found = true;
            break;
        }
    }
    assert(graphics_queue_found);

    free(queue_family_properties);

    return graphics_queue_index;
}

uint32_t renderer_get_present_queue(
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface)
{
    uint32_t present_queue_index;

    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        NULL
    );

    VkQueueFamilyProperties* queue_family_properties;
    queue_family_properties = malloc(
        queue_family_count * sizeof(*queue_family_properties)
    );

    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device,
        &queue_family_count,
        queue_family_properties
    );

    uint32_t i;
    VkBool32 wsi_support;
    bool present_queue_found = false;
    for (i=0; i<queue_family_count; i++)
    {
        VkResult wsi_query_result;
        wsi_query_result = vkGetPhysicalDeviceSurfaceSupportKHR(
            physical_device,
            i,
            surface,
            &wsi_support
        );
        assert(wsi_query_result == VK_SUCCESS);

        if (wsi_support) {
            present_queue_index = i;
            present_queue_found = true;
            break;
        }
    }
    assert(present_queue_found);

    free(queue_family_properties);

    return present_queue_index;
}

VkDevice renderer_get_device(
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface,
        VkPhysicalDeviceFeatures* required_features,
        uint32_t device_extension_count,
        const char** device_extensions)
{
    VkDevice device_handle;
    device_handle = VK_NULL_HANDLE;

    uint32_t graphics_family_index = renderer_get_graphics_queue(
        physical_device
    );
    uint32_t present_family_index = renderer_get_present_queue(
        physical_device,
        surface
    );

    uint32_t device_queue_count = 2;
    uint32_t device_queue_indices[] = {
        graphics_family_index,
        present_family_index
    };
    float device_queue_priorities[] = {1.0f, 1.0f};
    VkDeviceQueueCreateFlags device_queue_flags[] = {0, 0};

    VkDeviceQueueCreateInfo* device_queue_infos;
    device_queue_infos = malloc(
        sizeof(*device_queue_infos) * device_queue_count
    );

    uint32_t i;
    for (i=0; i<device_queue_count; i++) {
        VkDeviceQueueCreateInfo device_queue_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .flags = device_queue_flags[i],
            .queueFamilyIndex = device_queue_indices[i],
            .queueCount = 1,
            .pQueuePriorities = &device_queue_priorities[i]
        };
        device_queue_infos[i] = device_queue_info;
    }

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pQueueCreateInfos = device_queue_infos,
        .pEnabledFeatures = required_features,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = device_extension_count,
        .ppEnabledExtensionNames = (const char* const*)device_extensions
    };

    if (graphics_family_index == present_family_index)
        device_info.queueCreateInfoCount = 1;
    else
        device_info.queueCreateInfoCount = 2;

    VkResult result;
    result = vkCreateDevice(
        physical_device,
        &device_info,
        NULL,
        &device_handle
    );
    assert(result == VK_SUCCESS);

    free(device_queue_infos);

    return device_handle;
}

VkCommandPool renderer_get_command_pool(
        VkPhysicalDevice physical_device,
        VkDevice device)
{
    VkCommandPool command_pool_handle;
    command_pool_handle = VK_NULL_HANDLE;

    uint32_t graphics_family_index = renderer_get_graphics_queue(
        physical_device
    );

    VkCommandPoolCreateInfo command_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = graphics_family_index
    };

    VkResult result;
    result = vkCreateCommandPool(
        device,
        &command_pool_info,
        NULL,
        &command_pool_handle
    );
    assert(result == VK_SUCCESS);

    return command_pool_handle;
}

VkSurfaceFormatKHR renderer_get_image_format(
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface)
{
    VkSurfaceFormatKHR image_format;

    uint32_t format_count;
    VkSurfaceFormatKHR* formats;
    VkResult result;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device,
        surface,
        &format_count,
        NULL
    );
    assert(result == VK_SUCCESS);
    formats = malloc(format_count * sizeof(*formats));
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device,
        surface,
        &format_count,
        formats
    );
    assert(result == VK_SUCCESS);

    // Free to choose any format
    if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
    {
        image_format.format = VK_FORMAT_B8G8R8A8_UNORM;
        image_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
    // Limited selection of formats
    else
    {
        uint32_t i;
        bool ideal_format_found = false;
        for (i=0; i<format_count; i++)
        {
            // Ideal format B8G8R8A8_UNORM, SRGB_NONLINEAR
            if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
                formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                image_format = formats[i];
                ideal_format_found = true;
                break;
            }
        }

        if (!ideal_format_found)
        {
            image_format = formats[0];
        }
    }

    free(formats);

    return image_format;
}

VkExtent2D renderer_get_swapchain_extent(
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface,
        uint32_t window_width,
        uint32_t window_height)
{
    // Determine extent (resolution of swapchain images)
    VkExtent2D extent;
    extent.width = window_width;
    extent.height = window_height;

	VkSurfaceCapabilitiesKHR surface_capabilities;
    VkResult result;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physical_device,
        surface,
        &surface_capabilities
    );
    assert(result == VK_SUCCESS);

    if (surface_capabilities.currentExtent.width == UINT32_MAX)
    {
        extent.width = MAX(
            surface_capabilities.minImageExtent.width,
            MIN(surface_capabilities.maxImageExtent.width, extent.width)
        );

        extent.height = MAX(
            surface_capabilities.minImageExtent.height,
            MIN(surface_capabilities.maxImageExtent.height, extent.height)
        );
    }

    return extent;
}

VkSwapchainKHR renderer_get_swapchain(
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkSurfaceKHR surface,
        VkSurfaceFormatKHR image_format,
        VkExtent2D swapchain_extent,
        VkSwapchainKHR old_swapchain)
{
    VkSwapchainKHR swapchain_handle;
    swapchain_handle = VK_NULL_HANDLE;

    // Determine image count (maxImageCount 0 == no limit)
	VkSurfaceCapabilitiesKHR surface_capabilities;
    VkResult result;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physical_device,
        surface,
        &surface_capabilities
    );
    assert(result == VK_SUCCESS);

    uint32_t desired_image_count;
    desired_image_count = surface_capabilities.minImageCount + 1;
    if (desired_image_count > 0 &&
        desired_image_count > surface_capabilities.maxImageCount)
    {
        desired_image_count = surface_capabilities.maxImageCount;
    }

    // Queue family indices
    VkSharingMode sharing_mode;

    uint32_t queue_family_count;
    const uint32_t* queue_family_indices;

    uint32_t graphics_family_index = renderer_get_graphics_queue(
        physical_device
    );
    uint32_t present_family_index = renderer_get_present_queue(
        physical_device,
        surface
    );

    if (graphics_family_index != present_family_index)
    {
        sharing_mode = VK_SHARING_MODE_CONCURRENT;
        queue_family_count = 2;
        const uint32_t indices[] = {
            graphics_family_index,
            present_family_index
        };
        queue_family_indices = indices;
    }
    else
    {
        sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        queue_family_count = 0;
        queue_family_indices = NULL;
    }

    // Find a present mode, ideally MAILBOX unless unavailable,
    // in which case fall back to FIFO (always available)
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    VkPresentModeKHR* present_modes;
    uint32_t present_mode_count;

    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device,
        surface,
        &present_mode_count,
        NULL
    );

    present_modes = malloc(present_mode_count * sizeof(*present_modes));

    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device,
        surface,
        &present_mode_count,
        present_modes
    );

    uint32_t i;
    for (i=0; i<present_mode_count; i++)
    {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            present_mode = present_modes[i];
            break;
        }
    }

    free(present_modes);

    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .surface = surface,
        .minImageCount = desired_image_count,
        .imageFormat = image_format.format,
        .imageColorSpace = image_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = sharing_mode,
        .queueFamilyIndexCount = queue_family_count,
        .pQueueFamilyIndices = queue_family_indices,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain
    };

    result = vkCreateSwapchainKHR(
        device,
        &swapchain_info,
        NULL,
        &swapchain_handle
    );
    assert(result == VK_SUCCESS);

    if (old_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(
            device,
            old_swapchain,
            NULL
        );
    }

    return swapchain_handle;
}

uint32_t renderer_get_swapchain_image_count(
        VkDevice device,
        VkSwapchainKHR swapchain)
{
    uint32_t image_count;
    vkGetSwapchainImagesKHR(
        device,
        swapchain,
        &image_count,
        NULL
    );
    return image_count;
}

void renderer_create_swapchain_buffers(
        VkDevice device,
        VkCommandPool command_pool,
        VkSwapchainKHR swapchain,
        VkSurfaceFormatKHR swapchain_image_format,
        struct swapchain_buffer* swapchain_buffers,
        uint32_t swapchain_image_count)
{
    VkImage* images;
    images = malloc(swapchain_image_count * sizeof(*images));
    assert(images);

    VkResult result;
    result = vkGetSwapchainImagesKHR(
        device,
        swapchain,
        &swapchain_image_count,
        images
    );
    assert(result == VK_SUCCESS);

    VkCommandBufferAllocateInfo cmd_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .format = swapchain_image_format.format,
        .components = {
             .r = VK_COMPONENT_SWIZZLE_R,
             .g = VK_COMPONENT_SWIZZLE_G,
             .b = VK_COMPONENT_SWIZZLE_B,
             .a = VK_COMPONENT_SWIZZLE_A
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .viewType = VK_IMAGE_VIEW_TYPE_2D
    };

    uint32_t i;
    for (i=0; i<swapchain_image_count; i++)
    {
        result = vkAllocateCommandBuffers(
            device,
            &cmd_alloc_info,
            &swapchain_buffers[i].cmd
        );
        assert(result == VK_SUCCESS);

        swapchain_buffers[i].image = images[i];
        view_info.image = swapchain_buffers[i].image;

        result = vkCreateImageView(
            device,
            &view_info,
            NULL,
            &swapchain_buffers[i].image_view
        );
        assert(result == VK_SUCCESS);
    }
}

void renderer_submit_command_buffer(
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkQueue queue,
        VkCommandBuffer* cmd)
{
    vkEndCommandBuffer(*cmd);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = cmd,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL
    };

    VkResult result;
    result = vkQueueSubmit(
        queue,
        1,
        &submit_info,
        VK_NULL_HANDLE
    );
    assert(result == VK_SUCCESS);

    vkQueueWaitIdle(queue);
}

void renderer_change_image_layout(
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkQueue queue,
        VkCommandPool command_pool,
        VkImage image,
        VkImageLayout old_layout,
        VkImageLayout new_layout,
        VkAccessFlagBits src_access_mask,
        VkImageAspectFlags aspect_mask)
{
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cmd_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkResult result;
    result = vkAllocateCommandBuffers(
        device,
        &cmd_alloc_info,
        &cmd
    );
    assert(result == VK_SUCCESS);

    VkCommandBufferBeginInfo cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0,
        .pInheritanceInfo = NULL
    };

    result = vkBeginCommandBuffer(
        cmd,
        &cmd_begin_info
    );
    assert(result == VK_SUCCESS);

    VkImageMemoryBarrier memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = src_access_mask,
        .dstAccessMask = 0,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            aspect_mask,
            0,
            1,
            0,
            1
        }
    };

    if (new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    if (new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        memory_barrier.dstAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    if (new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        memory_barrier.dstAccessMask =
            VK_ACCESS_SHADER_READ_BIT |
            VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    }

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &memory_barrier
    );

    renderer_submit_command_buffer(
        physical_device,
        device,
        queue,
        &cmd
    );

    vkFreeCommandBuffers(
        device,
        command_pool,
        1,
        &cmd
    );
}

uint32_t renderer_find_memory_type(
        uint32_t memory_type_bits,
        VkMemoryPropertyFlags properties,
        uint32_t memory_type_count,
        VkMemoryType* memory_types)
{
    uint32_t memory_type;

    uint32_t i;
    bool memory_type_found = false;
    for (i=0; i<memory_type_count; ++i)
    {
        if ((memory_type_bits & (1 << i)) &&
            (memory_types[i].propertyFlags & properties) == properties)
        {
            memory_type = i;
            memory_type_found = true;
            break;
        }
    }

    assert(memory_type_found);

    return memory_type;
}

VkFormat renderer_get_depth_format(
        VkPhysicalDevice physicalDevice,
        VkImageTiling tiling,
        VkFormatFeatureFlags features)
{
    VkFormat format = VK_FORMAT_UNDEFINED;

	VkFormat formats[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    uint32_t format_count = sizeof(formats)/sizeof(formats[0]);

    uint32_t i;
    for (i=0; i<format_count; i++)
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(
            physicalDevice,
            formats[i],
            &properties
        );

        if ( (tiling == VK_IMAGE_TILING_LINEAR &&
             (properties.linearTilingFeatures & features) == features)
                ||
             (tiling == VK_IMAGE_TILING_OPTIMAL &&
             (properties.optimalTilingFeatures & features) == features) )
        {
            format = formats[i];
            break;
        }
    }

    return format;
}

struct renderer_image renderer_get_depth_image(
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkQueue queue,
        VkCommandPool command_pool,
        VkExtent2D extent,
        VkFormat depth_format)
{
    struct renderer_image depth_image;

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depth_format,
        .extent.width = extent.width,
        .extent.height = extent.height,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED
    };

    VkResult result;
    result = vkCreateImage(
        device,
        &image_info,
        NULL,
        &depth_image.image
    );
    assert(result == VK_SUCCESS);

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, depth_image.image, &mem_reqs);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    uint32_t mem_type;
    mem_type = renderer_find_memory_type(
        mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        mem_props.memoryTypeCount,
        mem_props.memoryTypes
    );

    VkMemoryAllocateInfo mem_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_type
    };

    result = vkAllocateMemory(
        device, &mem_alloc_info, NULL, &depth_image.memory);
    assert(result == VK_SUCCESS);

    result = vkBindImageMemory(
        device, depth_image.image, depth_image.memory, 0);
    assert(result == VK_SUCCESS);

    renderer_change_image_layout(
        physical_device,
        device,
        queue,
        command_pool,
        depth_image.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        0,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );

    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = depth_image.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depth_format,
        .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    result = vkCreateImageView(
        device,
        &image_view_info,
        NULL,
        &depth_image.image_view
    );
    assert(result == VK_SUCCESS);

    return depth_image;
}

VkRenderPass renderer_get_render_pass(
        VkDevice device,
        VkFormat image_format,
        VkFormat depth_format)
{
    VkRenderPass render_pass_handle;
    render_pass_handle = VK_NULL_HANDLE;

    VkAttachmentDescription color_desc = {
        .flags = 0,
        .format = image_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };
    VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentDescription depth_desc = {
        .flags = 0,
        .format = depth_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };
    VkAttachmentReference depth_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkAttachmentDescription attachments[] = {color_desc, depth_desc};

	VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .flags = 0,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pResolveAttachments = NULL,
        .pDepthStencilAttachment = &depth_ref,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL
    };

    VkSubpassDependency subpass_dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency
    };

    VkResult result;
    result = vkCreateRenderPass(
        device,
        &render_pass_info,
        NULL,
        &render_pass_handle
    );
    assert(result == VK_SUCCESS);

    return render_pass_handle;
}

void renderer_create_framebuffers(
        VkDevice device,
        VkRenderPass render_pass,
        VkExtent2D swapchain_extent,
        struct swapchain_buffer* swapchain_buffers,
        VkImageView depth_image_view,
        VkFramebuffer* framebuffers,
        uint32_t swapchain_image_count)
{
    VkImageView attachments[2];
    attachments[1] = depth_image_view;

    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = render_pass,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .width = swapchain_extent.width,
        .height = swapchain_extent.height,
        .layers = 1
    };

    uint32_t i;
	for (i=0; i<swapchain_image_count; i++)
    {
        attachments[0] = swapchain_buffers[i].image_view;

        VkResult result;
        result = vkCreateFramebuffer(
            device,
            &framebuffer_info,
            NULL,
            &framebuffers[i]
        );
        assert(result == VK_SUCCESS);
	}
}

struct renderer_buffer renderer_get_buffer(
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory_flags)
{
    struct renderer_buffer buffer;

    VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL
    };
    vkCreateBuffer(device, &buffer_info, NULL, &buffer.buffer);

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, buffer.buffer, &mem_reqs);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

	VkMemoryAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = renderer_find_memory_type(
			mem_reqs.memoryTypeBits,
            memory_flags,
			mem_props.memoryTypeCount,
			mem_props.memoryTypes
		)
	};

    VkResult result;
    result = vkAllocateMemory(
        device,
        &alloc_info,
        NULL,
        &buffer.memory
    );
    assert(result == VK_SUCCESS);

    vkBindBufferMemory(
        device,
        buffer.buffer,
        buffer.memory,
        0
    );

    return buffer;
}

struct renderer_buffer renderer_get_vertex_buffer(
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkQueue queue,
        VkCommandPool command_pool,
        struct renderer_vertex* vertices,
        uint32_t vertex_count)
{
    struct renderer_buffer vbo;
    struct renderer_buffer staging_vbo;

    VkDeviceSize mem_size = sizeof(*vertices) * vertex_count;

    staging_vbo = renderer_get_buffer(
        physical_device,
        device,
        mem_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    vkMapMemory(device, staging_vbo.memory, 0, mem_size, 0, &vbo.mapped);
    memcpy(vbo.mapped, vertices, (size_t)mem_size);
    vkUnmapMemory(device, staging_vbo.memory);

    vbo = renderer_get_buffer(
        physical_device,
        device,
        mem_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    VkCommandBuffer copy_cmd;
    VkCommandBufferAllocateInfo cmd_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkResult result;
    result = vkAllocateCommandBuffers(device, &cmd_alloc_info, &copy_cmd);
    assert(result == VK_SUCCESS);

    VkCommandBufferBeginInfo cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL
    };
    result = vkBeginCommandBuffer(copy_cmd, &cmd_begin_info);
    assert(result == VK_SUCCESS);

    VkBufferCopy region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = mem_size
    };

    vkCmdCopyBuffer(
        copy_cmd,
        staging_vbo.buffer,
        vbo.buffer,
        1,
        &region
    );

    renderer_submit_command_buffer(
        physical_device,
        device,
        queue,
        &copy_cmd
    );

    vkFreeCommandBuffers(
        device,
        command_pool,
        1,
        &copy_cmd
    );

    vkDestroyBuffer(device, staging_vbo.buffer, NULL);
    vkFreeMemory(device, staging_vbo.memory, NULL);

    return vbo;
}

struct renderer_buffer renderer_get_index_buffer(
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkQueue queue,
        VkCommandPool command_pool,
        uint32_t* indices,
        uint32_t index_count)
{
    struct renderer_buffer ibo;
    struct renderer_buffer staging_ibo;

    VkDeviceSize mem_size = sizeof(*indices) * index_count;

    staging_ibo = renderer_get_buffer(
        physical_device,
        device,
        mem_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    vkMapMemory(device, staging_ibo.memory, 0, mem_size, 0, &ibo.mapped);
    memcpy(ibo.mapped, indices, (size_t)mem_size);
    vkUnmapMemory(device, staging_ibo.memory);

    ibo = renderer_get_buffer(
        physical_device,
        device,
        mem_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    VkCommandBuffer copy_cmd;
    VkCommandBufferAllocateInfo cmd_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkResult result;
    result = vkAllocateCommandBuffers(device, &cmd_alloc_info, &copy_cmd);
    assert(result == VK_SUCCESS);

    VkCommandBufferBeginInfo cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL
    };
    result = vkBeginCommandBuffer(copy_cmd, &cmd_begin_info);
    assert(result == VK_SUCCESS);

    VkBufferCopy region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = mem_size
    };

    vkCmdCopyBuffer(
        copy_cmd,
        staging_ibo.buffer,
        ibo.buffer,
        1,
        &region
    );

    renderer_submit_command_buffer(
        physical_device,
        device,
        queue,
        &copy_cmd
    );

    vkFreeCommandBuffers(
        device,
        command_pool,
        1,
        &copy_cmd
    );

    vkDestroyBuffer(device, staging_ibo.buffer, NULL);
    vkFreeMemory(device, staging_ibo.memory, NULL);

    return ibo;
}

struct renderer_buffer renderer_get_uniform_buffer(
        VkPhysicalDevice physical_device,
        VkDevice device,
        struct renderer_buffer* staging_buffer)
{
    struct renderer_buffer uniform_buffer;
    uint32_t uniform_buffer_size = sizeof(float) * 16 * 3;

    *staging_buffer = renderer_get_buffer(
        physical_device,
        device,
        uniform_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    staging_buffer->size = uniform_buffer_size;

    uniform_buffer = renderer_get_buffer(
        physical_device,
        device,
        uniform_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    uniform_buffer.size = uniform_buffer_size;

    return uniform_buffer;
}

void renderer_update_uniform_buffer(
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkQueue queue,
        VkCommandPool command_pool,
        VkExtent2D swapchain_extent,
        struct renderer_buffer* uniform_buffer,
        struct renderer_buffer* staging_buffer)
{
    mat4x4 viewprojection[2];
    memset(viewprojection, 0, sizeof(viewprojection));

    vec3 eye = {12.0f, 12.0f, 12.0f};
    vec3 center = {0.0f, 0.0f, 0.0f};
    vec3 up = {0.0f, 0.0f, 1.0f};
    mat4x4_look_at(viewprojection[0], eye, center, up);

    float aspect = (float)swapchain_extent.width/swapchain_extent.height;

    mat4x4_perspective(viewprojection[1], 0.78f, aspect, 0.1f, 100.0f);
    viewprojection[1][1][1] *= -1;

    vkMapMemory(
        device,
        staging_buffer->memory,
        0,
        uniform_buffer->size,
        0,
        &uniform_buffer->mapped
    );
    memset(uniform_buffer->mapped, 0, 3*sizeof(mat4x4));
    ((float*)uniform_buffer->mapped)[16] = 1.f;
    ((float*)uniform_buffer->mapped)[21] = 1.f;
    ((float*)uniform_buffer->mapped)[26] = 1.f;
    ((float*)uniform_buffer->mapped)[31] = 1.f;
    memcpy(uniform_buffer->mapped, viewprojection, 2*sizeof(mat4x4));
    vkUnmapMemory(device, staging_buffer->memory);

    VkCommandBuffer copy_cmd;
    VkCommandBufferAllocateInfo cmd_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkResult result;
    result = vkAllocateCommandBuffers(device, &cmd_alloc_info, &copy_cmd);
    assert(result == VK_SUCCESS);

    VkCommandBufferBeginInfo cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL
    };
    result = vkBeginCommandBuffer(copy_cmd, &cmd_begin_info);
    assert(result == VK_SUCCESS);

    VkBufferCopy region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = uniform_buffer->size
    };

    vkCmdCopyBuffer(
        copy_cmd,
        staging_buffer->buffer,
        uniform_buffer->buffer,
        1,
        &region
    );

    renderer_submit_command_buffer(
        physical_device,
        device,
        queue,
        &copy_cmd
    );

    vkFreeCommandBuffers(
        device,
        command_pool,
        1,
        &copy_cmd
    );
}

struct renderer_image renderer_get_image(
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkExtent3D extent,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags memory_flags)
{
    struct renderer_image image;
    memset(&image, 0, sizeof(image));

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {extent.width, extent.height, extent.depth},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED
    };

    VkResult result;
    result = vkCreateImage(device, &image_info, NULL, &image.image);
    assert(result == VK_SUCCESS);

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, image.image, &mem_reqs);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

	VkMemoryAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = renderer_find_memory_type(
			mem_reqs.memoryTypeBits,
			memory_flags,
			mem_props.memoryTypeCount,
			mem_props.memoryTypes
		)
	};

    result = vkAllocateMemory(device, &alloc_info, NULL, &image.memory);
    assert(result == VK_SUCCESS);

    vkBindImageMemory(device, image.image, image.memory, 0);

    return image;
}

struct renderer_image renderer_load_texture(
    const char* src,
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkQueue queue,
    VkCommandPool command_pool)
{
    struct renderer_image tex_image;

    stbi_uc* pixels = NULL;
    int tex_width, tex_height, tex_channels;
    pixels = stbi_load(
        src,
        &tex_width,
        &tex_height,
        &tex_channels,
        STBI_rgb_alpha
    );
    assert(pixels && tex_width && tex_height);

    VkExtent3D extent = {.width = tex_width, .height = tex_height, .depth = 1};
    tex_image = renderer_get_image(
        physical_device,
        device,
        extent,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    tex_image.width = tex_width;
    tex_image.height = tex_height;
    tex_image.size = tex_width * tex_height * 4;

    struct renderer_buffer staging_buffer;
    staging_buffer = renderer_get_buffer(
        physical_device,
        device,
        tex_image.size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkResult result;
    result = vkMapMemory(
        device,
        staging_buffer.memory,
        0,
        tex_image.size,
        0,
        &tex_image.mapped
    );
    assert(result == VK_SUCCESS);
    memcpy(tex_image.mapped, pixels, (size_t)tex_image.size);
    vkUnmapMemory(device, staging_buffer.memory);
    stbi_image_free(pixels);

    renderer_change_image_layout(
        physical_device,
        device,
        queue,
        command_pool,
        tex_image.image,
        VK_IMAGE_LAYOUT_PREINITIALIZED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_ACCESS_HOST_WRITE_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    VkCommandBuffer copy_cmd;
    VkCommandBufferAllocateInfo cmd_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    result = vkAllocateCommandBuffers(device, &cmd_alloc_info, &copy_cmd);
    assert(result == VK_SUCCESS);

    VkCommandBufferBeginInfo cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL
    };
    result = vkBeginCommandBuffer(copy_cmd, &cmd_begin_info);
    assert(result == VK_SUCCESS);

	VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {tex_width, tex_height, 1}
    };

    vkCmdCopyBufferToImage(
        copy_cmd,
        staging_buffer.buffer,
        tex_image.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    renderer_submit_command_buffer(
        physical_device,
        device,
        queue,
        &copy_cmd
    );

    vkFreeCommandBuffers(
        device,
        command_pool,
        1,
        &copy_cmd
    );

    renderer_change_image_layout(
        physical_device,
        device,
        queue,
        command_pool,
        tex_image.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    vkDestroyBuffer(device, staging_buffer.buffer, NULL);
    vkFreeMemory(device, staging_buffer.memory, NULL);

    VkImageViewCreateInfo image_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.image = tex_image.image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.components = {
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY
		},
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel = 0,
		.subresourceRange.levelCount = 1,
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.layerCount = 1
    };
    result = vkCreateImageView(
        device,
        &image_view_info,
        NULL,
        &tex_image.image_view
    );
    assert(result == VK_SUCCESS);

	VkSamplerCreateInfo sampler_info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias =0.0f,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = 1.0f,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE
    };
    result = vkCreateSampler(
        device,
        &sampler_info,
        NULL,
        &tex_image.sampler
    );
    assert(result == VK_SUCCESS);

    return tex_image;
}

VkDescriptorPool renderer_get_descriptor_pool(
        VkDevice device)
{
    VkDescriptorPool descriptor_pool_handle;
    descriptor_pool_handle = VK_NULL_HANDLE;

    VkDescriptorPoolSize ubo_pool_size = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1
    };

    VkDescriptorPoolSize sampler_pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1
    };

    VkDescriptorPoolSize pool_sizes[] = {
        ubo_pool_size,
        sampler_pool_size
    };

    VkDescriptorPoolCreateInfo descriptor_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .maxSets = 1,
        .poolSizeCount = 2,
        .pPoolSizes = pool_sizes
    };

    VkResult result;
    result = vkCreateDescriptorPool(
        device,
        &descriptor_pool_info,
        NULL,
        &descriptor_pool_handle
    );
    assert(result == VK_SUCCESS);

    return descriptor_pool_handle;
}

VkDescriptorSetLayout renderer_get_descriptor_layout(
        VkDevice device)
{
    VkDescriptorSetLayout descriptor_layout_handle;
    descriptor_layout_handle = VK_NULL_HANDLE;

    VkDescriptorSetLayoutBinding ubo_layout_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = NULL
    };

    VkDescriptorSetLayoutBinding sampler_layout_binding = {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = NULL
    };

    VkDescriptorSetLayoutBinding layoutBindings[2] = {
        ubo_layout_binding,
        sampler_layout_binding
    };

    VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .bindingCount = 2,
        .pBindings = layoutBindings
    };

    VkResult result;
    result = vkCreateDescriptorSetLayout(
        device,
        &descriptor_layout_info,
        NULL,
        &descriptor_layout_handle
    );
    assert(result == VK_SUCCESS);

	return descriptor_layout_handle;
}

VkDescriptorSet renderer_get_descriptor_set(
        VkDevice device,
        VkDescriptorPool descriptor_pool,
        VkDescriptorSetLayout* descriptor_layouts,
        uint32_t descriptor_count,
        struct renderer_buffer* uniform_buffer,
        struct renderer_image* tex_image)
{
    VkDescriptorSet descriptor_set_handle;
    descriptor_set_handle = VK_NULL_HANDLE;

    VkDescriptorSetAllocateInfo descriptor_set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = descriptor_count,
        .pSetLayouts = descriptor_layouts
    };

    VkResult result;
    result = vkAllocateDescriptorSets(
        device,
        &descriptor_set_info,
        &descriptor_set_handle
    );
    assert(result == VK_SUCCESS);

	VkDescriptorBufferInfo buffer_info = {
        .buffer = uniform_buffer->buffer,
        .offset = 0,
        .range = uniform_buffer->size
    };

	VkDescriptorImageInfo image_info = {
        .sampler = tex_image->sampler,
        .imageView = tex_image->image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet ubo_descriptor_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = NULL,
        .dstSet = descriptor_set_handle,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = NULL,
        .pBufferInfo = &buffer_info,
        .pTexelBufferView = NULL
    };

    VkWriteDescriptorSet sampler_descriptor_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = NULL,
        .dstSet = descriptor_set_handle,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info,
        .pBufferInfo = NULL,
        .pTexelBufferView = NULL
    };

	VkWriteDescriptorSet descriptor_writes[] = {
        ubo_descriptor_write,
        sampler_descriptor_write
    };

    vkUpdateDescriptorSets(device, 2, descriptor_writes, 0, NULL);

    return descriptor_set_handle;
}

VkShaderModule renderer_get_shader_module(
        VkDevice device,
        const char* fname)
{
    VkShaderModule module;

    FILE* fp = fopen(fname, "r");
    assert(fp);

    uint32_t code_size;
    fseek(fp, 0L, SEEK_END);
    code_size = ftell(fp);
    rewind(fp);

    char* shader_code_buffer;
    shader_code_buffer = malloc(code_size);
    assert(shader_code_buffer);

    size_t bytes_read;
    bytes_read = fread(shader_code_buffer, 1, code_size, fp);
    assert(bytes_read);

    VkShaderModuleCreateInfo shader_module_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = code_size,
        .pCode = (uint32_t*)(shader_code_buffer)
    };

    VkResult result;
    result = vkCreateShaderModule(
        device,
        &shader_module_info,
        NULL,
        &module
    );
    assert(result == VK_SUCCESS);

    free(shader_code_buffer);

    return module;
}

VkPipelineShaderStageCreateInfo renderer_get_shader_stage(
        VkShaderStageFlagBits stage,
        VkShaderModule module)
{
    VkPipelineShaderStageCreateInfo shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = stage,
        .module = module,
        .pName = "main",
        .pSpecializationInfo = NULL
    };

    shader_stage_info.module = module;

    return shader_stage_info;
}

VkVertexInputBindingDescription renderer_get_binding_description(
        VkVertexInputRate vertex_input_rate)
{
    VkVertexInputBindingDescription binding_description = {
        .binding = 0,
        .stride = sizeof(struct renderer_vertex),
        .inputRate = vertex_input_rate
    };

    return binding_description;
}

VkVertexInputAttributeDescription renderer_get_attribute_description(
        uint32_t location,
        VkFormat format,
        uint32_t offset)
{
    VkVertexInputAttributeDescription attribute_description = {
        .location = location,
        .binding = 0,
        .format = format,
        .offset = offset
    };

    return attribute_description;
}

VkPipelineVertexInputStateCreateInfo renderer_get_vertex_input_state(
        VkVertexInputBindingDescription* binding_description,
        VkVertexInputAttributeDescription* attribute_descriptions,
        uint32_t attribute_description_count)
{
    VkPipelineVertexInputStateCreateInfo vertex_input_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = binding_description,
        .vertexAttributeDescriptionCount = attribute_description_count,
        .pVertexAttributeDescriptions = attribute_descriptions
    };

    return vertex_input_state;
}

VkPipelineInputAssemblyStateCreateInfo renderer_get_input_assembly_state()
{
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    return input_assembly_state;
}

VkPipelineTessellationStateCreateInfo renderer_get_tessellation_state(
        uint32_t patch_control_points)
{
    VkPipelineTessellationStateCreateInfo tesselation_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .pNext =  NULL,
        .flags = 0,
        .patchControlPoints = patch_control_points
    };

    return tesselation_state;
}

VkViewport renderer_get_viewport(
        float viewport_x,
        float viewport_y,
        VkExtent2D extent)
{
    VkViewport viewport = {
        .x = viewport_x,
        .y = viewport_y,
        .width = (float)extent.width,
        .height = (float)extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    return viewport;
}

VkRect2D renderer_get_scissor(
        float scissor_x,
        float scissor_y,
        VkExtent2D extent)
{
	VkRect2D scissor = {
		.offset = {scissor_x, scissor_y},
		.extent = extent
	};

    return scissor;
}


VkPipelineViewportStateCreateInfo renderer_get_viewport_state(
        VkViewport* viewports,
        uint32_t viewport_count,
        VkRect2D* scissors,
        uint32_t scissor_count)
{
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .viewportCount = viewport_count,
        .pViewports = viewports,
        .scissorCount = scissor_count,
        .pScissors = scissors
    };

    return viewport_state;
}

VkPipelineRasterizationStateCreateInfo renderer_get_rasterization_state(
        VkCullModeFlags cull_mode,
        VkFrontFace front_face)
{
    VkPipelineRasterizationStateCreateInfo rasterization_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = cull_mode,
        .frontFace = front_face,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f
    };

    return rasterization_state;
}

VkPipelineMultisampleStateCreateInfo renderer_get_multisample_state(
        VkSampleCountFlagBits rasterization_samples)
{
    VkPipelineMultisampleStateCreateInfo multisample_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .rasterizationSamples = rasterization_samples,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.0f,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    return multisample_state;
}

VkPipelineDepthStencilStateCreateInfo renderer_get_depth_stencil_state()
{
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front.failOp = 0,
        .front.passOp = 0,
        .front.depthFailOp = 0,
        .front.compareOp = 0,
        .front.compareMask = 0,
        .front.writeMask = 0,
        .front.reference = 0,
        .back.failOp = 0,
        .back.passOp = 0,
        .back.depthFailOp = 0,
        .back.compareOp = 0,
        .back.compareMask = 0,
        .back.writeMask = 0,
        .back.reference = 0,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    return depth_stencil_state;
}

VkPipelineColorBlendAttachmentState renderer_get_color_blend_attachment()
{
    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT
    };

    return color_blend_attachment;
}


VkPipelineColorBlendStateCreateInfo renderer_get_color_blend_state(
        VkPipelineColorBlendAttachmentState* attachments,
        uint32_t attachment_count)
{
    VkPipelineColorBlendStateCreateInfo color_blend_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = 0,
        .attachmentCount = attachment_count,
        .pAttachments = attachments,
        .blendConstants[0] = 0.0f,
        .blendConstants[1] = 0.0f,
        .blendConstants[2] = 0.0f,
        .blendConstants[3] = 0.0f
    };

    return color_blend_state;
}

/*VkPipelineDynamicStateCreateInfo renderer_get_dynamic_state()
{
}*/

VkPipelineLayout renderer_get_pipeline_layout(
        VkDevice device,
        VkDescriptorSetLayout* descriptor_layouts,
        uint32_t descriptor_layout_count,
        VkPushConstantRange* push_constant_ranges,
        uint32_t push_constant_range_count)
{
    VkPipelineLayout pipeline_layout_handle;
    pipeline_layout_handle = VK_NULL_HANDLE;

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = descriptor_layout_count,
        .pSetLayouts = descriptor_layouts,
        .pushConstantRangeCount = push_constant_range_count,
        .pPushConstantRanges = push_constant_ranges
    };

    VkResult result;
    result = vkCreatePipelineLayout(
        device,
        &layout_info,
        NULL,
        &pipeline_layout_handle
    );
    assert(result == VK_SUCCESS);

    return pipeline_layout_handle;
}

VkPipeline renderer_get_graphics_pipeline(
        VkDevice device,
        VkGraphicsPipelineCreateInfo* create_info)
{
    VkPipeline graphics_pipeline_handle;
    graphics_pipeline_handle = VK_NULL_HANDLE;

    VkResult result;
    result = vkCreateGraphicsPipelines(
        device,
        VK_NULL_HANDLE,
        1,
        create_info,
        NULL,
        &graphics_pipeline_handle
    );
    assert(result == VK_SUCCESS);

    return graphics_pipeline_handle;
}

VkPipeline renderer_get_base_graphics_pipeline(
        VkDevice device,
        VkExtent2D swapchain_extent,
        VkPipelineLayout pipeline_layout,
        VkRenderPass render_pass,
        uint32_t subpass)
{
    VkPipeline base_graphics_pipeline;

    VkShaderModule vert_shader_module;
    vert_shader_module = renderer_get_shader_module(
        device,
        "assets/shaders/vert.spv"
    );
    VkPipelineShaderStageCreateInfo vert_shader_stage;
    vert_shader_stage = renderer_get_shader_stage(
        VK_SHADER_STAGE_VERTEX_BIT,
        vert_shader_module
    );

    VkShaderModule frag_shader_module;
    frag_shader_module = renderer_get_shader_module(
        device,
        "assets/shaders/frag.spv"
    );
    VkPipelineShaderStageCreateInfo frag_shader_stage;
    frag_shader_stage = renderer_get_shader_stage(
        VK_SHADER_STAGE_FRAGMENT_BIT,
        frag_shader_module
    );

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vert_shader_stage,
        frag_shader_stage
    };
    uint32_t shader_stage_count = 2;

    VkVertexInputBindingDescription binding_description;
    binding_description = renderer_get_binding_description(
        VK_VERTEX_INPUT_RATE_VERTEX
    );

    VkVertexInputAttributeDescription position_attribute_description;
    position_attribute_description = renderer_get_attribute_description(
        0,
        VK_FORMAT_R32G32B32_SFLOAT,
        offsetof(struct renderer_vertex, x)
    );

    VkVertexInputAttributeDescription texture_attribute_description;
    texture_attribute_description = renderer_get_attribute_description(
        1,
        VK_FORMAT_R32G32_SFLOAT,
        offsetof(struct renderer_vertex, u)
    );

    VkVertexInputAttributeDescription attribute_descriptions[] = {
        position_attribute_description,
        texture_attribute_description
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_state;
    vertex_input_state = renderer_get_vertex_input_state(
        &binding_description,
        attribute_descriptions,
        2
    );

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
    input_assembly_state = renderer_get_input_assembly_state();

    VkViewport viewport = renderer_get_viewport(0, 0, swapchain_extent);
    VkRect2D scissor = renderer_get_scissor(0, 0, swapchain_extent);
    VkPipelineViewportStateCreateInfo viewport_state;
    viewport_state = renderer_get_viewport_state(
        &viewport, 1,
        &scissor, 1
    );

    VkPipelineRasterizationStateCreateInfo rasterization_state;
    rasterization_state = renderer_get_rasterization_state(
        VK_CULL_MODE_BACK_BIT,
        VK_FRONT_FACE_COUNTER_CLOCKWISE
    );

    VkPipelineMultisampleStateCreateInfo multisample_state;
    multisample_state = renderer_get_multisample_state(VK_SAMPLE_COUNT_1_BIT);

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
    depth_stencil_state = renderer_get_depth_stencil_state();

    VkPipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment = renderer_get_color_blend_attachment();
    VkPipelineColorBlendStateCreateInfo color_blend_state;
    color_blend_state = renderer_get_color_blend_state(
        &color_blend_attachment, 1
    );

    VkGraphicsPipelineCreateInfo base_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stageCount = shader_stage_count,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pTessellationState = NULL,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &depth_stencil_state,
        .pColorBlendState = &color_blend_state,
        .pDynamicState = NULL,
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = subpass,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    base_graphics_pipeline = renderer_get_graphics_pipeline(
        device,
        &base_pipeline_info
    );

    vkDestroyShaderModule(device, vert_shader_module, NULL);
    vkDestroyShaderModule(device, frag_shader_module, NULL);

    return base_graphics_pipeline;
}

void renderer_load_textured_model(
        struct renderer_resources* resources)
{
    struct renderer_image tex_image;
    tex_image = renderer_load_texture(
        "assets/textures/robot-texture.png",
        resources->physical_device,
        resources->device,
        resources->graphics_queue,
        resources->command_pool
    );

    resources->mesh.texture = malloc(sizeof(tex_image));
    assert(resources->mesh.texture);
    memcpy(resources->mesh.texture, &tex_image, sizeof(tex_image));

    resources->mesh.descriptor_set = renderer_get_descriptor_set(
        resources->device,
        resources->descriptor_pool,
        &resources->descriptor_layout,
        1,
        &resources->uniform_buffer,
        &tex_image
    );

    const struct aiScene* scene = NULL;
    scene = aiImportFile(
        "assets/models/robot.dae",
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices
    );
    assert(scene);

    struct aiMesh* mesh = scene->mMeshes[0];

    struct renderer_vertex* vertices;
    vertices = malloc(mesh->mNumVertices * sizeof(*vertices));
    assert(vertices);

    uint32_t i;
    for (i=0; i<mesh->mNumVertices; i++) {
        vertices[i].x = mesh->mVertices[i].x;
        vertices[i].y = mesh->mVertices[i].y;
        vertices[i].z = mesh->mVertices[i].z;
        vertices[i].u = mesh->mTextureCoords[0][i].x;
        vertices[i].v = mesh->mTextureCoords[0][i].y;
    }
    resources->mesh.vertex_count = i;

    resources->mesh.vbo = renderer_get_vertex_buffer(
        resources->physical_device,
        resources->device,
        resources->graphics_queue,
        resources->command_pool,
        vertices,
        resources->mesh.vertex_count
    );

    uint32_t* indices = malloc(mesh->mNumFaces * 3 * sizeof(*indices));
    assert(indices);

    for (i=0; i<mesh->mNumFaces * 3; i++) {
        indices[i] = mesh->mFaces[(int)(i/3.f)].mIndices[i%3];
    }
    resources->mesh.index_count = i;

    resources->mesh.ibo = renderer_get_index_buffer(
        resources->physical_device,
        resources->device,
        resources->graphics_queue,
        resources->command_pool,
        indices,
        resources->mesh.index_count
    );

    aiReleaseImport(scene);

    free(vertices);
    free(indices);
}

void renderer_record_draw_commands(
        VkPipeline pipeline,
        VkPipelineLayout pipeline_layout,
        VkRenderPass render_pass,
        VkExtent2D swapchain_extent,
        VkFramebuffer* framebuffers,
        struct swapchain_buffer* swapchain_buffers,
        uint32_t swapchain_image_count,
        struct renderer_mesh* mesh)
{
    VkCommandBufferBeginInfo cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        .pInheritanceInfo = NULL
    };

    VkClearValue clear_values[2];
    clear_values[0].color.float32[0] = 0.0f;
    clear_values[0].color.float32[1] = 0.0f;
    clear_values[0].color.float32[2] = 0.0f;
    clear_values[0].color.float32[3] = 1.0f;
    clear_values[1].depthStencil.depth = 1.0f;
    clear_values[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = render_pass,
        .renderArea.offset = {0,0},
        .renderArea.extent = {swapchain_extent.width, swapchain_extent.height},
        .clearValueCount = 2,
        .pClearValues = clear_values,
    };

    uint32_t i;
    for (i=0; i<swapchain_image_count; i++) {
        VkResult result;
        result = vkBeginCommandBuffer(
            swapchain_buffers[i].cmd,
            &cmd_begin_info
        );
        assert(result == VK_SUCCESS);

        render_pass_info.framebuffer = framebuffers[i];
        vkCmdBeginRenderPass(
            swapchain_buffers[i].cmd,
            &render_pass_info,
            VK_SUBPASS_CONTENTS_INLINE
        );

        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = swapchain_extent.width,
            .height = swapchain_extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vkCmdSetViewport(swapchain_buffers[i].cmd, 0, 1, &viewport);

        VkRect2D scissor = {
            .offset = {0,0},
            .extent = {swapchain_extent.width, swapchain_extent.height}
        };
        vkCmdSetScissor(swapchain_buffers[i].cmd, 0, 1, &scissor);

        vkCmdBindPipeline(
            swapchain_buffers[i].cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline
        );

        VkDeviceSize offsets[] = {0};

        vkCmdBindVertexBuffers(
            swapchain_buffers[i].cmd,
            0,
            1,
            &mesh->vbo.buffer,
            offsets
        );

        vkCmdBindIndexBuffer(
            swapchain_buffers[i].cmd,
            mesh->ibo.buffer,
            0,
            VK_INDEX_TYPE_UINT32
        );

        vkCmdBindDescriptorSets(
            swapchain_buffers[i].cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline_layout,
            0,
            1,
            &mesh->descriptor_set,
            0,
            NULL
        );

        vkCmdDrawIndexed(
            swapchain_buffers[i].cmd,
            mesh->index_count,
            1,
            0,
            0,
            0
        );

        vkCmdEndRenderPass(swapchain_buffers[i].cmd);

        result = vkEndCommandBuffer(swapchain_buffers[i].cmd);
        assert(result == VK_SUCCESS);
    }
}

VkSemaphore renderer_get_semaphore(
        VkDevice device)
{
    VkSemaphore semaphore_handle;

    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0
    };

    VkResult result;
    result = vkCreateSemaphore(
        device,
        &semaphore_info,
        NULL,
        &semaphore_handle
    );
    assert(result == VK_SUCCESS);

    return semaphore_handle;
}
