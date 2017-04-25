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

VkInstance get_vk_instance()
{
    VkInstance instance;

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
    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

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
        &instance
    );
    assert(result == VK_SUCCESS);

    for (i=0; i<all_extension_count; i++)
        free(all_extensions[i]);
    free(all_extensions);
    free(available_layers);

    return instance;
}
