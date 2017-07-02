#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "renderer.h"

#define APP_NAME "Cave Explorer 2"
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0
#define APP_VERSION_PATCH 0

#define VALIDATION_ENABLED 1

void renderer_prepare_vk(GLFWwindow* window)
{
    // Creation
    VkInstance instance;
    instance = renderer_get_vk_instance();
    assert(instance != VK_NULL_HANDLE);

    PFN_vkCreateDebugReportCallbackEXT fp_create_debug_callback;
    fp_create_debug_callback =
            (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
                    instance,
                    "vkCreateDebugReportCallbackEXT"
            );
    assert(*fp_create_debug_callback);

    VkDebugReportCallbackEXT debug_callback_ext;
    debug_callback_ext = renderer_get_debug_callback(
        instance,
        fp_create_debug_callback
    );

    VkSurfaceKHR surface;
    surface = renderer_get_vk_surface(
        instance,
        window
    );
    assert(surface != VK_NULL_HANDLE);

    const char* device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    uint32_t device_extension_count = 1;

    VkPhysicalDevice physical_device;
    physical_device = renderer_get_vk_physical_device(
        instance,
        surface,
        device_extension_count,
        device_extensions
    );
    assert(physical_device != VK_NULL_HANDLE);

	VkPhysicalDeviceFeatures required_features;
    memset(&required_features, VK_FALSE, sizeof(required_features));
    required_features.geometryShader = VK_TRUE;
    required_features.tessellationShader = VK_TRUE;
    required_features.fullDrawIndexUint32 = VK_TRUE;
    required_features.imageCubeArray = VK_TRUE;
    required_features.sampleRateShading = VK_TRUE;
    required_features.samplerAnisotropy = VK_TRUE;

    VkDevice device;
    device = renderer_get_vk_device(
        physical_device,
        surface,
        &required_features,
        device_extension_count,
        device_extensions
    );
    assert(device != VK_NULL_HANDLE);

    VkSurfaceFormatKHR image_format;
    image_format = renderer_get_vk_image_format(
        physical_device,
        surface
    );

    VkExtent2D image_extent;
    image_extent = renderer_get_vk_image_extent(
        physical_device,
        surface,
        640,
        480
    );

    VkSwapchainKHR swapchain;
    swapchain = renderer_get_vk_swapchain(
        physical_device,
        device,
        surface,
        image_format,
        image_extent,
        VK_NULL_HANDLE
    );
    assert(swapchain != VK_NULL_HANDLE);

    struct swapchain_buffer* swapchain_buffers = NULL;
    uint32_t image_count;
    renderer_create_swapchain_buffers(
        device,
        swapchain,
        image_format,
        &swapchain_buffers,
        &image_count
    );

    VkCommandPool command_pool;
    command_pool = renderer_get_vk_command_pool(
        physical_device,
        device
    );
    assert(command_pool != VK_NULL_HANDLE);

    VkFormat depth_format;
    depth_format = renderer_get_vk_depth_format(
        physical_device,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
    assert(depth_format != VK_FORMAT_UNDEFINED);

    struct texture_image depth_image;
    depth_image = renderer_get_depth_image(
        physical_device,
        device,
        command_pool,
        image_extent,
        depth_format
    );

    VkRenderPass render_pass;
    render_pass = renderer_get_vk_render_pass(
        device,
        image_format.format,
        depth_format
    );
    assert(render_pass != VK_NULL_HANDLE);

    VkFramebuffer* framebuffers = NULL;
    framebuffers = malloc(image_count * sizeof(*framebuffers));
    assert(framebuffers);
    renderer_create_framebuffers(
        device,
        render_pass,
        image_extent,
        swapchain_buffers,
        depth_image.image_view,
        framebuffers,
        image_count
    );

    /*struct texture_image my_texture;
    my_texture = renderer_load_texture(
        "../assets/test.png",
        physical_device,
        device,
        command_pool
    );*/

    VkDescriptorPool descriptor_pool;
    descriptor_pool = renderer_get_descriptor_pool(device);
    assert(descriptor_pool != VK_NULL_HANDLE);

    VkDescriptorSetLayout descriptor_layout;
    descriptor_layout = renderer_get_descriptor_layout(device);
    assert(descriptor_layout != VK_NULL_HANDLE);

    printf("Vulkan initialized successfully\n");

    // Destruction
    uint32_t i;

    vkDestroyDescriptorSetLayout(device, descriptor_layout, NULL);
    vkDestroyDescriptorPool(device, descriptor_pool, NULL);

    /*vkDestroyImage(device, my_texture.image, NULL);
    vkDestroyImageView(device, my_texture.image_view, NULL);
    vkFreeMemory(device, my_texture.memory, NULL);
    vkDestroySampler(device, my_texture.sampler, NULL);*/

    for (i=0; i<image_count; i++) {
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
    }

    vkDestroyRenderPass(device, render_pass, NULL);

    vkDestroyImage(device, depth_image.image, NULL);
    vkDestroyImageView(device, depth_image.image_view, NULL);
    vkFreeMemory(device, depth_image.memory, NULL);

    vkDestroyCommandPool(device, command_pool, NULL);

    for (i=0; i<image_count; i++) {
        vkDestroyImageView(
            device,
            swapchain_buffers[i].image_view,
            NULL
        );
    }
    free(swapchain_buffers);

    vkDestroySwapchainKHR(
        device,
        swapchain,
        NULL
    );

    vkDestroyDevice(device, NULL);

    vkDestroySurfaceKHR(instance, surface, NULL);

    PFN_vkDestroyDebugReportCallbackEXT fp_destroy_debug_callback;
    fp_destroy_debug_callback =
            (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
                    instance,
                    "vkDestroyDebugReportCallbackEXT"
            );
    assert(*fp_destroy_debug_callback);
    fp_destroy_debug_callback(instance, debug_callback_ext, NULL);

    vkDestroyInstance(instance, NULL);

    printf("Vulkan destroyed successfully\n");
}

VkInstance renderer_get_vk_instance()
{
    VkInstance instance_handle;
    instance_handle = VK_NULL_HANDLE;

	// Create info
    VkInstanceCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;

    // Application info
    VkApplicationInfo app_info;
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = NULL;
    app_info.pApplicationName = APP_NAME;
    app_info.applicationVersion = VK_MAKE_VERSION(
            APP_VERSION_MAJOR,
            APP_VERSION_MINOR,
            APP_VERSION_PATCH);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0,0,0);
    app_info.apiVersion = VK_API_VERSION_1_0;
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

    VkDebugReportCallbackCreateInfoEXT debug_info;
    debug_info.sType =
        VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_info.pNext = NULL;
    debug_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                        VK_DEBUG_REPORT_WARNING_BIT_EXT;
    debug_info.pUserData = NULL;
    debug_info.pfnCallback = &renderer_debug_callback;

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

    fprintf(stderr, "%s\n", message);

    free(message);

    return VK_FALSE;
}

VkSurfaceKHR renderer_get_vk_surface(
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

VkPhysicalDevice renderer_get_vk_physical_device(
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

VkDevice renderer_get_vk_device(
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

    VkDeviceCreateInfo device_info;
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = NULL;
    device_info.flags = 0;
    device_info.pQueueCreateInfos = device_queue_infos;
    if (graphics_family_index == present_family_index)
    {
        device_info.queueCreateInfoCount = 1;
    }
    else
    {
        device_info.queueCreateInfoCount = 2;
    }
    device_info.pEnabledFeatures = required_features;
    device_info.enabledLayerCount = 0;
    device_info.ppEnabledLayerNames = NULL;
    device_info.enabledExtensionCount = device_extension_count;
    device_info.ppEnabledExtensionNames =
        (const char* const*)device_extensions;

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

VkSurfaceFormatKHR renderer_get_vk_image_format(
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

VkExtent2D renderer_get_vk_image_extent(
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

VkSwapchainKHR renderer_get_vk_swapchain(
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkSurfaceKHR surface,
        VkSurfaceFormatKHR image_format,
        VkExtent2D image_extent,
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

    VkSwapchainCreateInfoKHR swapchain_info;
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.pNext = NULL;
    swapchain_info.flags = 0;
    swapchain_info.surface = surface;
    swapchain_info.minImageCount = desired_image_count;
    swapchain_info.imageFormat = image_format.format;
    swapchain_info.imageColorSpace = image_format.colorSpace;
    swapchain_info.imageExtent = image_extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = sharing_mode;
    swapchain_info.queueFamilyIndexCount = queue_family_count;
    swapchain_info.pQueueFamilyIndices = queue_family_indices;
    swapchain_info.preTransform = surface_capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = present_mode;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = old_swapchain;

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

void renderer_create_swapchain_buffers(
        VkDevice device,
        VkSwapchainKHR swapchain,
        VkSurfaceFormatKHR image_format,
        struct swapchain_buffer** swapchain_buffers,
        uint32_t* image_count)
{
    vkGetSwapchainImagesKHR(
        device,
        swapchain,
        image_count,
        NULL
    );

    VkImage* images = NULL;
    images = malloc((*image_count) * sizeof(*images));
    assert(images);

    *swapchain_buffers = malloc((*image_count) * sizeof(**swapchain_buffers));
    assert(*swapchain_buffers);

    vkGetSwapchainImagesKHR(
        device,
        swapchain,
        image_count,
        images
    );

    uint32_t i;
    for (i=0; i<(*image_count); i++)
    {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .image = images[i],
            .format = image_format.format,
            .components = {
                 .r = VK_COMPONENT_SWIZZLE_R,
                 .g = VK_COMPONENT_SWIZZLE_G,
                 .b = VK_COMPONENT_SWIZZLE_B,
                 .a = VK_COMPONENT_SWIZZLE_A,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
        };

        (*swapchain_buffers)[i].image = images[i];

        VkResult result;
        result = vkCreateImageView(
            device,
            &view_info,
            NULL,
            &((*swapchain_buffers)[i].image_view)
        );
        assert(result == VK_SUCCESS);
    }

    free(images);
}

VkCommandPool renderer_get_vk_command_pool(
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

void renderer_change_image_layout(
        VkDevice device,
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
        memory_barrier.dstAccessMask =
            VK_ACCESS_TRANSFER_READ_BIT;
    }

    if (new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        memory_barrier.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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
            ((memory_types[i].propertyFlags & properties) == properties))
        {
            memory_type = i;
            memory_type_found = true;
        }
    }

    assert(memory_type_found);

    return memory_type;
}

VkFormat renderer_get_vk_depth_format(
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

struct texture_image renderer_get_depth_image(
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkCommandPool command_pool,
        VkExtent2D extent,
        VkFormat depth_format)
{
    struct texture_image depth_image;

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
        device,
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

VkRenderPass renderer_get_vk_render_pass(
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

void renderer_create_image(
        VkPhysicalDevice physical_device,
        VkDevice device,
        stbi_uc* pixels,
        struct texture_image* tex_image,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags memory_flags)
{
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {tex_image->width, tex_image->height, 1},
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
    result = vkCreateImage(device, &image_info, NULL, &tex_image->image);
    assert(result == VK_SUCCESS);

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, tex_image->image, &mem_reqs);

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

    result = vkAllocateMemory(device, &alloc_info, NULL, &tex_image->memory);
    assert(result == VK_SUCCESS);

    vkBindImageMemory(device, tex_image->image, tex_image->memory, 0);

    stbi_image_free(pixels);
}

struct texture_image renderer_load_texture(
    const char* src,
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkCommandPool command_pool)
{
    struct texture_image tex_image;

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

    tex_image.width = tex_width;
    tex_image.height = tex_height;
    tex_image.size = tex_width * tex_height * 4;

    struct texture_image staging_image;
    staging_image.width = tex_width;
    staging_image.height = tex_height;
    staging_image.size = tex_width * tex_height * 4;

    renderer_create_image(
        physical_device,
        device,
        pixels,
        &staging_image,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_LINEAR,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkResult result;
    result = vkMapMemory(
        device,
        staging_image.memory,
        0,
        tex_image.size,
        0,
        tex_image.mapped
    );
    assert(result == VK_SUCCESS);
    memcpy(tex_image.mapped, pixels, (size_t)tex_image.size);
    vkUnmapMemory(device, staging_image.memory);

    renderer_create_image(
        physical_device,
        device,
        pixels,
        &staging_image,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    renderer_change_image_layout(
        device,
        command_pool,
        staging_image.image,
        VK_IMAGE_LAYOUT_PREINITIALIZED,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    renderer_change_image_layout(
        device,
        command_pool,
        tex_image.image,
        VK_IMAGE_LAYOUT_PREINITIALIZED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,
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

	VkImageCopy image_copy = {
        .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .srcOffset = {0, 0, 0},
        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .dstOffset = {0, 0, 0},
        .extent = {tex_width, tex_height, 1}
    };
    vkCmdCopyImage(
        copy_cmd,
        staging_image.image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        tex_image.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &image_copy
    );

    renderer_change_image_layout(
        device,
        command_pool,
        tex_image.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    vkDestroyImage(device, staging_image.image, NULL);
    vkFreeMemory(device, staging_image.memory, NULL);

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
		.maxAnisotropy = 16,
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

struct buffer renderer_get_uniform_buffer(
        VkPhysicalDevice physical_device,
        VkDevice device)
{
    struct buffer uniform_buffer;

	uniform_buffer.size = sizeof(float) * 16 * 3; // 3 mat4

    VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.size = uniform_buffer.size,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL
    };
    vkCreateBuffer(device, &buffer_info, NULL, &uniform_buffer.buffer);

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, uniform_buffer.buffer, &mem_reqs);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

	VkMemoryAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = renderer_find_memory_type(
			mem_reqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			mem_props.memoryTypeCount,
			mem_props.memoryTypes
		)
	};

    VkResult result;
    result = vkAllocateMemory(
        device,
        &alloc_info,
        NULL,
        &uniform_buffer.memory
    );
    assert(result == VK_SUCCESS);

    vkBindBufferMemory(
        device,
        uniform_buffer.buffer,
        uniform_buffer.memory,
        0
    );

    return uniform_buffer;
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
        .poolSizeCount = sizeof(pool_sizes)/sizeof(pool_sizes[0]),
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

    VkDescriptorSetLayoutBinding uboLayoutBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = NULL
    };

    VkDescriptorSetLayoutBinding samplerLayoutBinding = {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = NULL
    };

    VkDescriptorSetLayoutBinding layoutBindings[2] = {
        uboLayoutBinding,
        samplerLayoutBinding
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

/*VkDescriptorSet renderer_get_descriptor_set(
        VkDevice device,
        VkDescriptorPool descriptor_pool,
        VkDescriptorSetLayout* descriptor_layouts
        uint32_t descriptor_count,
        VkBuffer uniform_buffer,
        Vk
        )
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

	VkDescriptorBufferInfo buffer_info;
    bufferInfo.buffer = uniform_buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(struct UniformBufferObject);

	VkDescriptorImageInfo imageInfo;
    imageInfo.sampler = engine->textureSampler;
    imageInfo.imageView = engine->textureImageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet ubo_descriptor_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = NULL,
        .dstSet = engine->descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = NULL,
        .pBufferInfo = &bufferInfo,
        .pTexelBufferView = NULL
    };

    VkWriteDescriptorSet sampler_descriptor_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = NULL,
        .dstSet = engine->descriptorSet,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo,
        .pBufferInfo = NULL,
        .pTexelBufferView = NULL
    };

	VkWriteDescriptorSet descriptor_writes[] = {
        ubo_descriptor_write,
        sampler_descriptor_write
    };

    vkUpdateDescriptorSets(device, 2, descriptor_writes, 0, NULL);

    return descriptor_set_handle;
}*/
