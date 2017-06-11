#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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

    printf("Vulkan initialized successfully\n");

    // Destruction
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

VkDevice renderer_get_vk_device(
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface,
        VkPhysicalDeviceFeatures* required_features,
        uint32_t device_extension_count,
        const char** device_extensions)
{
    VkDevice device_handle;
    device_handle = VK_NULL_HANDLE;

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

    int graphics_family_index = -1;
    int present_family_index = -1;

    VkBool32 wsi_support;
    uint32_t i;
    for (i=0; i<queue_family_count; i++)
    {
        if (queue_family_properties[i].queueCount > 0 &&
            queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            graphics_family_index = i;
        }

        VkResult wsi_query_result;
        wsi_query_result = vkGetPhysicalDeviceSurfaceSupportKHR(
            physical_device,
            i,
            surface,
            &wsi_support
        );
        assert(wsi_query_result == VK_SUCCESS);
        if (wsi_support) {
            present_family_index = i;
        }

        if (graphics_family_index >= 0 &&
            present_family_index >= 0)
        {
            break;
        }
    }
    assert(graphics_family_index >= 0);
    assert(present_family_index >= 0);

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

    return image_format;
}

/*void VKIcreateSwapchain(
        struct VKIswapchainDetails* swapchainDetails,
        struct VKIqueueFamilyIndices* queueFamilyIndices,
        VkSurfaceKHR surface,
        VkDevice device,
        VkSwapchainKHR* swapchain,
        uint32_t* swapchainImageCount,
        struct VKIswapchainBuffer* swapchainBuffers,
        VkFormat* swapchainImageFormat,
        VkExtent2D* swapchainExtent)
{
    VkSwapchainKHR swapchain_handle;
    swapchain_handle = VK_NULL_HANDLE;

    // Determine image count (maxImageCount 0 == no limit)
	VkSurfaceCapabilitiesKHR surface_capabilities;
    VkResult result;
    result = vkGetPhysicalDeviceSurfaceCapabilitesKHR(
        physical_device,
        surface,
        &surface_capabilities
    );
    assert(result == VK_SUCCESS);

    uint32_t image_count;
    image_count = surface_capabilities.minImageCount + 1;
    if (image_count > 0 &&
        image_count > surface_capabilities.maxImageCount)
    {
        image_count = surface_capabilities.maxImageCount;
    }


    // Determine extent (resolution of swapchain images)
    VkExtent2D extent;
    extent.width = WIDTH;
    extent.height = HEIGHT;
    if (swapchainDetails->capabilities.currentExtent.width == UINT32_MAX)
    {
        extent.width = MAX(
            swapchainDetails->capabilities.minImageExtent.width,
            MIN(
                swapchainDetails->capabilities.maxImageExtent.width,
                extent.width
            )
        );

        extent.height = MAX(
            swapchainDetails->capabilities.minImageExtent.height,
            MIN(
                swapchainDetails->capabilities.maxImageExtent.height,
                extent.height
            )
        );
    }
    *swapchainExtent = extent;

    // Queue family indices
    VkSharingMode sharingMode;
    uint32_t queueFamilyIndexCount;
    const uint32_t* pQueueFamilyIndices;

    const uint32_t indices[2] = {
        (uint32_t)queueFamilyIndices->graphicsFamily,
        (uint32_t)queueFamilyIndices->presentFamily
    };

    if (queueFamilyIndices->graphicsFamily !=
        queueFamilyIndices->presentFamily)
    {
        sharingMode = VK_SHARING_MODE_CONCURRENT;
        queueFamilyIndexCount = 2;
        pQueueFamilyIndices = indices;
    }
    else
    {
        sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        queueFamilyIndexCount = 0;
        pQueueFamilyIndices = NULL;
    }

    // Find a present mode, ideally MAILBOX unless unavailable,
    // in which case fall back to FIFO (always available)
    VkPresentModeKHR presentMode;
    uint32_t i;
    bool idealPresentModeFound = false;
    for (i=0; i<swapchainDetails->presentModeCount; i++)
    {
        if (swapchainDetails->presentModes[i] ==
                VK_PRESENT_MODE_MAILBOX_KHR)
        {
            presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            idealPresentModeFound = true;
        }
    }

    if (!idealPresentModeFound)
    {
        presentMode = VK_PRESENT_MODE_FIFO_KHR;
    }

    // Old swapchain
    VkSwapchainKHR oldSwapchain = *swapchain;

    VkSwapchainCreateInfoKHR swapchainInfo;
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.pNext = NULL;
    swapchainInfo.flags = 0;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageSharingMode = sharingMode;
    swapchainInfo.queueFamilyIndexCount = queueFamilyIndexCount;
    swapchainInfo.pQueueFamilyIndices = pQueueFamilyIndices;
    swapchainInfo.preTransform =
        swapchainDetails->capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR newSwapchain;
    VkResult result;
    result = vkCreateSwapchainKHR(
                device,
                &swapchainInfo,
                NULL,
                &newSwapchain
            );
    assert(result == VK_SUCCESS);

    *swapchain = newSwapchain;

    if (oldSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(
            device,
            oldSwapchain,
            NULL
        );
    }

    vkGetSwapchainImagesKHR(
        device,
        *swapchain,
        swapchainImageCount,
        NULL
    );
    VkImage* images;
    images = malloc((*swapchainImageCount) * sizeof(*images));
    assert(images);
    vkGetSwapchainImagesKHR(
        device,
        *swapchain,
        swapchainImageCount,
        images
    );

    // Images + views
    assert(!swapchainBuffers);
    swapchainBuffers = malloc(
        (*swapchainImageCount) * sizeof(*swapchainBuffers)
    );
    assert(swapchainBuffers);

    for (i=0; i<(*swapchainImageCount); i++)
    {
        swapchainBuffers[i].image = images[i];

        VkImageViewCreateInfo viewInfo;
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.pNext = NULL;
        viewInfo.flags = 0;
        viewInfo.image = swapchainBuffers[i].image;
        viewInfo.viewType = VK_IMAGE_TYPE_2D;
        viewInfo.format = *swapchainImageFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkResult result;
        result = vkCreateImageView(
                    device,
                    &viewInfo,
                    NULL,
                    &swapchainBuffers[i].imageView
                );
        assert(result == VK_SUCCESS);
    }

    free(images);
}*/
