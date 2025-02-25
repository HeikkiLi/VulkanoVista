#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "stb_image.h"
#include "Texture.h"
//#include "Vertex.h"

#include "MeshModel.h"
#include "ImGuiManager.h"

class Device;
class Swapchain;
class Window;
class Instance;
class Mesh;
struct Model;

struct FrameStats {
    float fps = 0.0f;
    float mspf = 0.0f; // milliseconds per frame
};

class Renderer
{
public:
    ~Renderer();
    void setup(Device* device, Swapchain* swapchain, Window* window, Instance* instance);
    void finalizeSetup();
    void drawFrame();
    void update(float deltaTime);
    void cleanup();
    
    void recreateSwapchain(VkExtent2D newExtent);

    Texture* getTexture(const std::string& texturePath);
    void cleanupTextures();

    int createMeshModel(std::string modelPath, std::string modelFile);
    MeshModel& getMeshModel(size_t index) { return modelList[index]; }
    VkRenderPass getRenderPass() { return renderPass; }

private:
    void createRenderPass();
    void createDescriptorSetLayout();
    void createPushConstantRange();
    void createGraphicsPipeline();
    void createColorBufferImage();
    void createDepthBufferImage();
    void createFramebuffers();
    void createCommandBuffers();
    void createTextureSampler();
    void createSyncObjects();

    void createDescriptorPools();
    void createDescriptorSets();
    void createInputDescriptorSets();
    int createTextureDescriptor(VkImageView textureImage);

    void createUniformBuffers();
    void updateUniformBuffers(uint32_t imageIndex);
      
    void cleanupSwapchain();

    VkPipelineShaderStageCreateInfo createShaderStage(const std::string& filepath, VkShaderStageFlagBits stage);

    void allocateDynamicBufferTransferSpace();

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, 
                    VkImageUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
                    VkImage* image, VkDeviceMemory* imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkFormat findDepthFormat();
    VkFormat findColorFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    void loadTexture(const std::string& filePath, Texture& texture);
    void loadTextureImage(const std::string& filePath, Texture& texture);

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    // init ImGui manager
    int initImGui();


    void calcFrameStats(float deltaTime);

    //--------------------------------------------------------------------------------
    // Render Frame methods
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    VkCommandBuffer getCurrentCommandBuffer() const;

    // Reference to external objects (set in setup)
    Device* device = nullptr;       // Pointer to Device for easy access
    Swapchain* swapchain = nullptr; // Pointer to Swapchain for easy access
    Window* window = nullptr;
    Instance* instance = nullptr;

    // Vulkan resources
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    // second pass pipeline
    VkPipelineLayout secondPipelineLayout = VK_NULL_HANDLE;
    VkPipeline secondPipeline = VK_NULL_HANDLE;

    // Framebuffers for each swapchain image
    std::vector<VkFramebuffer> framebuffers;

    // Command buffers, one per framebuffer
    // Command buffers hold GPU commands, such as drawing calls, memory transfers, and synchronization instructions.
    std::vector<VkCommandBuffer> commandBuffers;


    //-------------------------------------------------
    // Buffers for sub passes

    // Color buffer image
    std::vector<VkImage> colorBufferImages;
    std::vector<VkDeviceMemory> colorBufferImageMemory;
    std::vector<VkImageView> colorBufferImageViews;

    // Depth buffer
    std::vector<VkImage> depthBufferImages;
    std::vector<VkDeviceMemory> depthBufferImageMemory;
    std::vector<VkImageView> depthBufferImageViews;

    //-----------------------------------------------

    // Synchronization objects
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;  // Tracks the current frame in flight

    std::vector<VkShaderModule> shaderModules; // To store created shader modules

    // Descriptors
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets; // for viewProjection one for each swapchain image

    VkDescriptorSetLayout inputSetLayout;
    VkDescriptorPool inputDescriptorPool;
    std::vector<VkDescriptorSet> inputDescriptorSets;

    // texture sampler descriptor pool
    VkDescriptorPool samplerDescriptorPool;
    VkDescriptorSetLayout samplerSetLayout;
    std::vector<VkDescriptorSet> samplerDescriptorSets;

    VkPushConstantRange pushConstantRange;

    // View Projection uniform buffer for every swapchain image
    std::vector<VkBuffer> vpUniformBuffers;
    std::vector<VkDeviceMemory> vpUniformBuffersMemory;

    // Model dynamic uniform buffers
    std::vector<VkBuffer> modelDynUniformBuffers;
    std::vector<VkDeviceMemory> modelDynUniformBuffersMemory;
    //VkDeviceSize minUniformBufferOffset;
    //size_t modelUniformAlignment;

    //Model* modelTransferSpace;

    struct UboViewProjection {
        glm::mat4 projection;
        glm::mat4 view;
    } uboViewProjection;

    // Textures
    std::unordered_map<std::string, Texture> textures;
    VkSampler textureSampler;

    // MeshModels
    std::vector<MeshModel> modelList;
    float rotation = 0.0f;

    // ImGuiManager
    ImGuiManager* imguiManager = nullptr;

    FrameStats frameStats;
};
