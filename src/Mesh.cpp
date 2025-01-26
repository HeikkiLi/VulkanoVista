#include "Mesh.h"

#include "Utils.h"

Mesh::Mesh(Device* device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    : device(device), vertexBuffer(VK_NULL_HANDLE), vertexBufferMemory(VK_NULL_HANDLE),
    indexBuffer(VK_NULL_HANDLE), indexBufferMemory(VK_NULL_HANDLE), indexCount(indices.size()), vertexCount(vertices.size()) {
    createVertexBuffer(vertices);
    createIndexBuffer(indices);
    
    uboModel.model = glm::mat4(1.0f);
}


Mesh::~Mesh() 
{
    vkDestroyBuffer(device->getLogicalDevice(), vertexBuffer, nullptr);
    vkFreeMemory(device->getLogicalDevice(), vertexBufferMemory, nullptr);
    vkDestroyBuffer(device->getLogicalDevice(), indexBuffer, nullptr);
    vkFreeMemory(device->getLogicalDevice(), indexBufferMemory, nullptr);
}

void Mesh::setModel(glm::mat4 model)
{
    uboModel.model = model;
}

UboModel Mesh::getModel()
{
    return uboModel;
}


void Mesh::createVertexBuffer(const std::vector<Vertex>& vertices) 
{
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device->getLogicalDevice(), device->getPhysicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device->getLogicalDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device->getLogicalDevice(), stagingBufferMemory);

    createBuffer(device->getLogicalDevice(), device->getPhysicalDevice(), bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    device->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(device->getLogicalDevice(), stagingBuffer, nullptr);
    vkFreeMemory(device->getLogicalDevice(), stagingBufferMemory, nullptr);
}

void Mesh::createIndexBuffer(const std::vector<uint32_t>& indices) 
{
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device->getLogicalDevice(), device->getPhysicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device->getLogicalDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device->getLogicalDevice(), stagingBufferMemory);

    createBuffer(device->getLogicalDevice(), device->getPhysicalDevice(), bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    device->copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(device->getLogicalDevice(), stagingBuffer, nullptr);
    vkFreeMemory(device->getLogicalDevice(), stagingBufferMemory, nullptr);
}

// Bind the vertex and index buffers to the command buffer.
void Mesh::bind(VkCommandBuffer commandBuffer)
{
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::draw(VkCommandBuffer commandBuffer) 
{
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
}
