#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "Device.h"
#include "Vertex.h"



class Mesh {
public:
    Mesh(Device* device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    ~Mesh();

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);

private:
    void createVertexBuffer(const std::vector<Vertex>& vertices);
    void createIndexBuffer(const std::vector<uint32_t>& indices);

    Device* device;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indexCount;
};


