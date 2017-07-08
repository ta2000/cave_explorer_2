#ifndef RENDERER_H_
#define RENDERER_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "stb_image.h"

#include <stdbool.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

struct vertex
{
    float x,y,z;
};

struct renderer_image
{
    VkImage image;
    VkImageView image_view;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkSampler sampler;
    uint32_t width;
    uint32_t height;
    void* mapped;
};

struct renderer_buffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
};

struct swapchain_buffer
{
    VkImage image;
    VkImageView image_view;
    VkCommandBuffer cmd_buffer;
};

void renderer_prepare_vk(GLFWwindow* window);

VkInstance renderer_get_vk_instance();

VkDebugReportCallbackEXT renderer_get_debug_callback(
    VkInstance instance,
    PFN_vkCreateDebugReportCallbackEXT fp_create_debug_callback
);
VKAPI_ATTR VkBool32 VKAPI_CALL renderer_debug_callback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT obj_type,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* p_layer_prefix,
    const char* p_msg,
    void* p_user_data
);

VkSurfaceKHR renderer_get_vk_surface(
    VkInstance instance,
    GLFWwindow* window
);

VkPhysicalDevice renderer_get_vk_physical_device(
    VkInstance instance,
    VkSurfaceKHR surface,
    uint32_t device_extension_count,
    const char** device_extensions
);
bool physical_device_extensions_supported(
    VkPhysicalDevice physical_device,
    uint32_t required_extension_count,
    const char** required_extensions
);

uint32_t renderer_get_graphics_queue(
    VkPhysicalDevice physical_device
);

uint32_t renderer_get_present_queue(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface
);

VkDevice renderer_get_vk_device(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    VkPhysicalDeviceFeatures* required_features,
    uint32_t device_extension_count,
    const char** device_extensions
);

VkSurfaceFormatKHR renderer_get_vk_image_format(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface
);

VkExtent2D renderer_get_vk_image_extent(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    uint32_t window_width,
    uint32_t window_height
);

VkSwapchainKHR renderer_get_vk_swapchain(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkSurfaceKHR surface,
    VkSurfaceFormatKHR image_format,
    VkExtent2D image_extent,
    VkSwapchainKHR old_swapchain
);

void renderer_create_swapchain_buffers(
    VkDevice device,
    VkSwapchainKHR swapchain,
    VkSurfaceFormatKHR image_format,
    struct swapchain_buffer** swapchain_buffers,
    uint32_t* swapchain_buffer_count
);

VkCommandPool renderer_get_vk_command_pool(
    VkPhysicalDevice physical_device,
    VkDevice device
);

void renderer_submit_command_buffer(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkCommandBuffer* cmd
);

void renderer_change_image_layout(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkCommandPool command_pool,
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkAccessFlagBits src_access_mask,
    VkImageAspectFlags aspect_mask
);

uint32_t renderer_find_memory_type(
    uint32_t memory_type_bits,
    VkMemoryPropertyFlags properties,
    uint32_t memory_type_count,
    VkMemoryType* memoryTypes
);

VkFormat renderer_get_vk_depth_format(
    VkPhysicalDevice physical_device,
    VkImageTiling tiling,
    VkFormatFeatureFlags features
);

struct renderer_image renderer_get_depth_image(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkCommandPool command_pool,
    VkExtent2D extent,
    VkFormat depth_format
);

VkRenderPass renderer_get_vk_render_pass(
    VkDevice device,
    VkFormat image_format,
    VkFormat depth_format
);

void renderer_create_framebuffers(
    VkDevice device,
    VkRenderPass render_pass,
    VkExtent2D swapchain_extent,
    struct swapchain_buffer* swapchain_buffers,
    VkImageView depth_image_view,
    VkFramebuffer* framebuffers,
    uint32_t swapchain_image_count
);

struct renderer_buffer renderer_get_buffer(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memory_flags
);

struct renderer_image renderer_get_image(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkExtent3D extent,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_flags
);

struct renderer_buffer renderer_get_uniform_buffer(
    VkPhysicalDevice physical_device,
    VkDevice device
);

struct renderer_image renderer_load_texture(
    const char* src,
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkCommandPool command_pool
);

VkDescriptorPool renderer_get_descriptor_pool(
    VkDevice device
);

VkDescriptorSetLayout renderer_get_descriptor_layout(
    VkDevice device
);

VkDescriptorSet renderer_get_descriptor_set(
    VkDevice device,
    VkDescriptorPool descriptor_pool,
    VkDescriptorSetLayout* descriptor_layouts,
    uint32_t descriptor_count,
    struct renderer_buffer* uniform_buffer,
    struct renderer_image* tex_image
);

#endif
