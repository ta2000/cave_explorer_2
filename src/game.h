#ifndef GAME_H_
#define GAME_H_

struct vk_renderer_properties;
struct gl_renderer_properties;

struct game
{
    struct vk_renderer_properties* vk_props;
    struct gl_renderer_properties* gl_props;
    void (*setup_renderer)(struct game*);
};

struct vk_renderer_properties
{
    VkInstance instance;
    VkPhysicalDevice gpu;
    VkDevice device;

    VkPipelineLayout base_pipeline_layout;
    VkPipeline base_pipeline_handle;
};

struct gl_renderer_properties
{
    int tmp;
};

void game_init(struct game* self);
void game_setup_vk_renderer(struct game* self);
void game_setup_gl_renderer(struct game* self);

#endif
