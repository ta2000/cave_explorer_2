#ifndef RENDERER_H_
#define RENDERER_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "stb_image.h"

#include <stdbool.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

struct renderer_vertex
{
    float x,y,z;
    float u,v;
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
    void* mapped;
};

struct swapchain_buffer
{
    VkImage image;
    VkImageView image_view;
    VkCommandBuffer cmd;
};

struct renderer_mesh
{
    struct renderer_buffer vbo;
    uint32_t vertex_count;
    struct renderer_buffer ibo;
    uint32_t index_count;
    VkDescriptorSet descriptor_set;
    struct renderer_image* texture;
};

struct renderer_resources
{
    VkInstance instance;
    VkDebugReportCallbackEXT debug_callback_ext;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkExtent2D swapchain_extent;
    struct swapchain_buffer* swapchain_buffers;
    uint32_t swapchain_image_count;
    VkCommandPool command_pool;
    struct renderer_image depth_image;
    VkRenderPass render_pass;
    VkFramebuffer* framebuffers;
    struct renderer_buffer uniform_buffer;
    struct renderer_buffer staging_uniform_buffer;
    VkPipelineLayout base_graphics_pipeline_layout;
    VkPipeline base_graphics_pipeline;
    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkQueue graphics_queue;
    VkQueue present_queue;

    struct renderer_mesh mesh;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_layout;
};

void renderer_create_resources(
    struct renderer_resources* resources,
    GLFWwindow* window
);

void renderer_render(
    struct renderer_resources* resources
);

void renderer_destroy_resources(
    struct renderer_resources* resources
);

VkInstance renderer_get_instance();

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

VkSurfaceKHR renderer_get_surface(
    VkInstance instance,
    GLFWwindow* window
);

VkPhysicalDevice renderer_get_physical_device(
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

VkDevice renderer_get_device(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    VkPhysicalDeviceFeatures* required_features,
    uint32_t device_extension_count,
    const char** device_extensions
);

VkCommandPool renderer_get_command_pool(
    VkPhysicalDevice physical_device,
    VkDevice device
);

VkSurfaceFormatKHR renderer_get_image_format(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface
);

VkExtent2D renderer_get_swapchain_extent(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    uint32_t window_width,
    uint32_t window_height
);

VkSwapchainKHR renderer_get_swapchain(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkSurfaceKHR surface,
    VkSurfaceFormatKHR image_format,
    VkExtent2D swapchain_extent,
    VkSwapchainKHR old_swapchain
);

uint32_t renderer_get_swapchain_image_count(
    VkDevice device,
    VkSwapchainKHR swapchain
);

void renderer_create_swapchain_buffers(
    VkDevice device,
    VkCommandPool command_pool,
    VkSwapchainKHR swapchain,
    VkSurfaceFormatKHR swapchain_image_format,
    struct swapchain_buffer* swapchain_buffers,
    uint32_t swapchain_buffer_count
);

void renderer_submit_command_buffer(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkQueue queue,
    VkCommandBuffer* cmd
);

void renderer_change_image_layout(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkQueue queue,
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

VkFormat renderer_get_depth_format(
    VkPhysicalDevice physical_device,
    VkImageTiling tiling,
    VkFormatFeatureFlags features
);

struct renderer_image renderer_get_depth_image(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkQueue queue,
    VkCommandPool command_pool,
    VkExtent2D extent,
    VkFormat depth_format
);

VkRenderPass renderer_get_render_pass(
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

struct renderer_buffer renderer_get_vertex_buffer(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkQueue queue,
    VkCommandPool command_pool,
    struct renderer_vertex* vertices,
    uint32_t vertex_count
);

struct renderer_buffer renderer_get_index_buffer(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkQueue queue,
    VkCommandPool command_pool,
    uint32_t* indices,
    uint32_t index_count
);

struct renderer_buffer renderer_get_uniform_buffer(
    VkPhysicalDevice physical_device,
    VkDevice device,
    struct renderer_buffer* staging_buffer
);

void renderer_update_uniform_buffer(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkQueue queue,
    VkCommandPool command_pool,
    VkExtent2D swapchain_extent,
    struct renderer_buffer* uniform_buffer,
    struct renderer_buffer* staging_buffer
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

struct renderer_image renderer_load_texture(
    const char* src,
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkQueue queue,
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

VkShaderModule renderer_get_shader_module(
    VkDevice device,
    const char* fname
);

VkPipelineShaderStageCreateInfo renderer_get_shader_stage(
    VkShaderStageFlagBits stage,
    VkShaderModule module
);

VkVertexInputBindingDescription renderer_get_binding_description(
    VkVertexInputRate vertex_input_rate
);

VkVertexInputAttributeDescription renderer_get_attribute_description(
    uint32_t location,
    VkFormat format,
    uint32_t offset
);

VkPipelineVertexInputStateCreateInfo renderer_get_vertex_input_state(
    VkVertexInputBindingDescription* binding_description,
    VkVertexInputAttributeDescription* attribute_descriptions,
    uint32_t attribute_description_count
);

VkPipelineInputAssemblyStateCreateInfo renderer_get_input_assembly_state();

VkPipelineTessellationStateCreateInfo renderer_get_tessellation_state(
    uint32_t patch_control_points
);

VkViewport renderer_get_viewport(
    float viewport_x,
    float viewport_y,
    VkExtent2D extent
);

VkRect2D renderer_get_scissor(
    float scissor_x,
    float scissor_y,
    VkExtent2D extent
);

VkPipelineViewportStateCreateInfo renderer_get_viewport_state(
    VkViewport* viewports,
    uint32_t viewport_count,
    VkRect2D* scissors,
    uint32_t scissor_count
);

VkPipelineRasterizationStateCreateInfo renderer_get_rasterization_state(
    VkCullModeFlags cull_mode,
    VkFrontFace front_face
);

VkPipelineMultisampleStateCreateInfo renderer_get_multisample_state(
    VkSampleCountFlagBits rasterization_samples
);

VkPipelineDepthStencilStateCreateInfo renderer_get_depth_stencil_state();

VkPipelineColorBlendAttachmentState renderer_get_color_blend_attachment();

VkPipelineColorBlendStateCreateInfo renderer_get_color_blend_state(
    VkPipelineColorBlendAttachmentState* attachments,
    uint32_t attachment_count
);

VkPipelineLayout renderer_get_pipeline_layout(
    VkDevice device,
    VkDescriptorSetLayout* descriptor_layouts,
    uint32_t descriptor_layout_count,
    VkPushConstantRange* push_contant_ranges,
    uint32_t push_constant_range_count
);

VkPipeline renderer_get_graphics_pipeline(
    VkDevice device,
    VkGraphicsPipelineCreateInfo* create_info
);

VkPipeline renderer_get_base_graphics_pipeline(
    VkDevice device,
    VkExtent2D swapchain_extent,
    VkPipelineLayout pipeline_layout,
    VkRenderPass render_pass,
    uint32_t subpass
);

void renderer_load_textured_model(
    struct renderer_resources* resources
);

void renderer_record_draw_commands(
    VkPipeline pipeline,
    VkPipelineLayout pipeline_layout,
    VkRenderPass render_pass,
    VkExtent2D swapchain_extent,
    VkFramebuffer* framebuffers,
    struct swapchain_buffer* swapchain_buffers,
    uint32_t swapchain_image_count,
    struct renderer_mesh* mesh
);

VkSemaphore renderer_get_semaphore(
    VkDevice device
);

#endif
