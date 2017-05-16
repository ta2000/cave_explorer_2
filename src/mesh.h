#ifndef MESH_H_
#define MESH_H_

struct mesh
{
    VkBuffer vbo;
    VkDeviceMemory
    VkBuffer ibo;
    uint32_t* vertices;
    uint32_t* indices;
    float transform[4];
};

#endif
