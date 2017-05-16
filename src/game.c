#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "renderer.h"
#include "game.h"

void game_init(struct game* self)
{
    memset(self, 0, sizeof(*self));

	glfwInit();

    assert(glfwVulkanSupported() == GLFW_TRUE);
    self->setup_renderer = &game_setup_vk_renderer;
}

void game_setup_vk_renderer(struct game* self)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(
        640, 480,
        "Vulkan Window",
        NULL,
        NULL
    );
    assert(window);

    //glfwSetWindowUserPointer(window, self);
    //glfwSetWindowSizeCallback(window, onWindowResized);
    //glfwSetKeyCallback(window, keyCallback);

    renderer_prepare_vk(window);

    printf("Vulkan initialized successfully.\n");
}
