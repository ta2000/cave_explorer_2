#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "renderer.h"
#include "game.h"

void game_init(struct game* self)
{
    memset(self, 0, sizeof(*self));

	glfwInit();

    if (glfwVulkanSupported() == GLFW_TRUE) {
        printf("Vulkan supported.\n");
        self->setup_renderer = &game_setup_vk_renderer;
    } else {
        printf("Vulkan not supported. Using OpenGL instead.\n");
        self->setup_renderer = &game_setup_gl_renderer;
    }
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

    VkInstance instance = get_vk_instance();
    vkDestroyInstance(instance, NULL);

    printf("Vulkan initialized successfully.\n");
}

void game_setup_gl_renderer(struct game* self)
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    GLFWwindow* window = glfwCreateWindow(
        640, 480,
        "OpenGL Window",
        NULL,
        NULL
    );
    assert(window);

    printf("OpenGL initialized successfully.\n");
}
