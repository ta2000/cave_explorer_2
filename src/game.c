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
}

void game_setup_renderer(struct game* self)
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

    struct renderer_resources resources;
    renderer_create_resources(&resources, window);
    printf("Renderer created prepared successfully.\n");
    renderer_destroy_resources(&resources);
    printf("Renderer resources destroyed successfully.\n");

    glfwDestroyWindow(window);
}
