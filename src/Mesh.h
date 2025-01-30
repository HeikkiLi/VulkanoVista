#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>

#include "Device.h"
#include "Vertex.h"

class Renderer;
struct Texture;

struct Model {
    glm::mat4 model;
};


class Mesh {
public:
    Mesh(Device* device, Renderer* renderer, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& texturePath);
    ~Mesh();

    void setModelTransform(glm::mat4 transform);
    Model getModel();

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);

    Texture* getTexture() { return texture; }

private:
    void createVertexBuffer(const std::vector<Vertex>& vertices);
    void createIndexBuffer(const std::vector<uint32_t>& indices);

    Device* device;

    Model model;

    int vertexCount;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t indexCount;

    Texture* texture;
};
