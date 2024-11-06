#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>

//#include "Vertex.h"

class Device;
class Swapchain;
class Window;
class Mesh;

class Renderer
{
public:
    ~Renderer();
    void setup(Device* device, Swapchain* swapchain, Window* window);
    void drawFrame();
    void cleanup();
    
    void recreateSwapchain(VkExtent2D newExtent);

    VkBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void addMesh(std::shared_ptr<Mesh> mesh);

private:
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandBuffers();
    void createSyncObjects();
    
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    
    void cleanupSwapchain();

    VkPipelineShaderStageCreateInfo createShaderStage(const std::string& filepath, VkShaderStageFlagBits stage);

    //void createVertexBuffer();

    // Reference to external objects (set in setup)
    Device* device = nullptr;       // Pointer to Device for easy access
    Swapchain* swapchain = nullptr; // Pointer to Swapchain for easy access
    Window* window = nullptr;

    // Vulkan resources
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    // Framebuffers for each swapchain image
    std::vector<VkFramebuffer> framebuffers;

    // Command buffers, one per framebuffer
    // Command buffers hold GPU commands, such as drawing calls, memory transfers, and synchronization instructions.
    std::vector<VkCommandBuffer> commandBuffers;

    // Synchronization objects
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;  // Tracks the current frame in flight

    std::vector<VkShaderModule> shaderModules; // To store created shader modules


    std::vector<std::shared_ptr<Mesh>> meshes;

};
