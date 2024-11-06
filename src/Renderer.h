#pragma once

#include <vulkan/vulkan.h>

#include "Device.h"
#include "Swapchain.h"

class Renderer
{
public:
    ~Renderer();
    void setup(const Device& device, const Swapchain& swapchain);
    void drawFrame();
    void cleanup();

private:
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void recreateSwapchain();
    void cleanupSwapchain();

    VkPipelineShaderStageCreateInfo createShaderStage(const std::string& filepath, VkShaderStageFlagBits stage);


    // Reference to external objects (set in setup)
    Device* device = nullptr;       // Pointer to Device for easy access
    Swapchain* swapchain = nullptr; // Pointer to Swapchain for easy access

    // Vulkan resources
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;

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
};
