#include "Renderer.h"

#include <stdexcept>
#include <fstream>
#include <vector>
#include <array>

#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Utils.h"
#include "Device.h"
#include "Swapchain.h"
#include "Window.h"
#include "Mesh.h"
#include "Vertex.h"
#include "Logger.h"


Renderer::~Renderer()
{
    cleanup();
}

void Renderer::setup(Device* device,  Swapchain* swapchain, Window* window, Instance* instance)
{
    this->device = device;          // Store pointer to Device
    this->swapchain = swapchain;    // Store pointer to Swapchain
    this->window = window;          // and window..
    this->instance = instance;

    device->createCommandPool();

    createRenderPass();
    createDescriptorSetLayout();
    createPushConstantRange();
    
    createGraphicsPipeline();

    // create image buffers
    createColorBufferImage();
    createDepthBufferImage();
    
    createFramebuffers();

    createCommandBuffers();
    createTextureSampler();
    createSyncObjects();

    // Set minimum uniform buffer offset
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device->getPhysicalDevice(), &deviceProperties);
    
    // not in use with push constants
    //minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
    
    // not called with push constants, use this with uniform dynamic buffers.
    //allocateDynamicBufferTransferSpace();

    // view projection
    uboViewProjection.projection = glm::perspective(glm::radians(45.0f), (float)swapchain->getExtent().width / (float)swapchain->getExtent().height, 0.1f, 100.0f);
    
    uboViewProjection.view = glm::lookAt(
        glm::vec3(0.0f, 3.0f, 5.0f),  
        glm::vec3(0.0f, 1.0f, 0.0f),   
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    uboViewProjection.projection[1][1] *= -1;

    createUniformBuffers();
    createDescriptorPools();
    createDescriptorSets();
    createInputDescriptorSets();

    if (initImGui() == EXIT_FAILURE)
    {
        throw std::runtime_error("Failed to init ImGui!");
    }
}

void Renderer::finalizeSetup()
{
    //createDescriptorPools();
    //createDescriptorSets();
}


void Renderer::drawFrame()
{

    vkWaitForFences(device->getLogicalDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(device->getLogicalDevice(), 1, &inFlightFences[currentFrame]);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device->getLogicalDevice(), swapchain->getSwapchain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(swapchain->getExtent());
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    // Record commands to command buffer (for this specific image)
    updateUniformBuffers(imageIndex);
    recordCommandBuffer(commandBuffers[imageIndex], imageIndex);

    // Set up submit info for queue submission
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(device->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // Present the image
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = { swapchain->getSwapchain() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(device->getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain(window->getExtent());
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::update(float deltaTime) 
{
    calcFrameStats(deltaTime);

    for (size_t i = 0; i < modelList.size(); i++)
    {
        glm::mat4 originalModel  = modelList[i].getModel().model;

        glm::vec3 position = glm::vec3(originalModel[3]); 
        glm::vec3 scale = glm::vec3(
            glm::length(glm::vec3(originalModel[0])),  // X scale
            glm::length(glm::vec3(originalModel[1])),  // Y scale
            glm::length(glm::vec3(originalModel[2]))   // Z scale
        );

        float direction = (i == 0) ? -1.0f : 1.0f;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);  // Keep original position
        model = glm::rotate(model, glm::radians(rotation * direction), glm::vec3(0.0f, 1.0f, 0.0f)); // Apply rotation
        model = glm::scale(model, scale); // Keep original scale
        modelList[i].setModel(model);
    }
}

void Renderer::calcFrameStats(float deltaTime)
{
    static float elapsedTime = 0.0f;
    static int frameCount = 0;

    elapsedTime += deltaTime;
    frameCount++;

    // Update stats every 1 second
    if (elapsedTime >= 1.0f) 
    {
        frameStats.fps = static_cast<float>(frameCount) / elapsedTime;
        frameStats.mspf = (elapsedTime * 1000.0f) / frameCount;

        frameCount = 0;
        elapsedTime = 0.0f;
    }
}

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) 
    {
        throw std::runtime_error("Failed to begin command buffer!");
    }
    
    // Render pass begin
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapchain->getExtent();

    std::array<VkClearValue, 3> clearValues = {};
    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
    clearValues[1].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[2].depthStencil.depth = 1.0f;

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind graphics pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    
    // Iterate over all models and draw them
    for (size_t i = 0; i < modelList.size(); i++) 
    {
        // push constants to given shader.
        MeshModel meshModel = modelList[i];
        Model model = meshModel.getModel();
        vkCmdPushConstants(commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(Model),
            &model);

        for (size_t j = 0; j < meshModel.getMeshCount(); ++j)
        {

            // Bind mesh index and vertex buffers       
            meshModel.getMesh(j)->bind(commandBuffer);

            // Dynamic offset amount
            //uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * i;

            std::array<VkDescriptorSet, 2> descriptorSetGroup = {
                                                                descriptorSets[imageIndex],
                                                                samplerDescriptorSets[meshModel.getMesh(j)->getTextId()]
            };

            // bind descriptor sets
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), 0, nullptr);


            meshModel.getMesh(j)->draw(commandBuffer);  // Issue indexed draw call
        }
    }

    // Start ImGui frame
    imguiManager->beginFrame();

    // Render ImGui UI
    ImGui::Begin("Vulkan Engine");
    ImGui::Text("Hello from ImGui!");
    ImGui::SliderFloat("rotation", &rotation, 0.0f, 360.0f, "%.1f");
    ImGui::End();

    ImGui::Begin("Framerate", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::SetWindowSize(ImVec2(200, 30));
    ImGui::SetWindowPos(ImVec2(2, 2));
    ImGui::Text("%.3f ms/frame (%.1f FPS)", frameStats.mspf, frameStats.fps);
    ImGui::End();

    imguiManager->endFrame(commandBuffer);

    // start second subpass
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipelineLayout,
        0, 1, &inputDescriptorSets[imageIndex], 0, nullptr);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    // end the render pass
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

void Renderer::createInputDescriptorSets()
{
    inputDescriptorSets.resize(swapchain->getImageCount());

    std::vector<VkDescriptorSetLayout> setLayouts(swapchain->getImageCount(), inputSetLayout);

    VkDescriptorSetAllocateInfo setAllocInfo = {};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = inputDescriptorPool;
    setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapchain->getImageCount());
    setAllocInfo.pSetLayouts = setLayouts.data();

    // Alocate descriptor sets
    VkResult result = vkAllocateDescriptorSets(device->getLogicalDevice(), &setAllocInfo, inputDescriptorSets.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate input descriptor sets!");
    }

    // update each descriptor set with input attachment
    for (size_t i = 0; i < swapchain->getImageCount(); ++i)
    {
        // color attachment
        VkDescriptorImageInfo colourAttachmentDescriptor = {};
        colourAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        colourAttachmentDescriptor.imageView = colorBufferImageViews[i];
        colourAttachmentDescriptor.sampler = VK_NULL_HANDLE;

        // color attachment desc write
        VkWriteDescriptorSet colorWrite = {};
        colorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        colorWrite.dstSet = inputDescriptorSets[i];
        colorWrite.dstBinding = 0;
        colorWrite.dstArrayElement = 0;
        colorWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        colorWrite.descriptorCount = 1;
        colorWrite.pImageInfo = &colourAttachmentDescriptor;

        // depth attachment
        VkDescriptorImageInfo depthAttachmentDescimageInfo = {};
        depthAttachmentDescimageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        depthAttachmentDescimageInfo.imageView = depthBufferImageViews[i];
        depthAttachmentDescimageInfo.sampler = VK_NULL_HANDLE;

        // depth attachment desc write
        VkWriteDescriptorSet depthWrite = {};
        depthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        depthWrite.dstSet = inputDescriptorSets[i];
        depthWrite.dstBinding = 1;
        depthWrite.dstArrayElement = 0;
        depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        depthWrite.descriptorCount = 1;
        depthWrite.pImageInfo = &depthAttachmentDescimageInfo;

        std::vector<VkWriteDescriptorSet> setWrites = { colorWrite, depthWrite };
        vkUpdateDescriptorSets(device->getLogicalDevice(), static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
    }

}


void Renderer::recreateSwapchain(VkExtent2D windowExtent) 
{
    // Wait until the device is idle
    vkDeviceWaitIdle(device->getLogicalDevice());
    // Cleanup swapchain-related resources
    cleanupSwapchain();

    // Recreate swapchain with the updated extent
    swapchain->create(device, window->getSurface(), windowExtent);
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandBuffers();
}

Texture* Renderer::getTexture(const std::string& texturePath)
{
    if (textures.find(texturePath) != textures.end()) 
    {
        // return existing texture
        return &textures[texturePath]; 
    }

    // Load new texture
    Texture texture;
    loadTexture(texturePath, texture);
    textures[texturePath] = texture;

    return &textures[texturePath];
}

void Renderer::cleanupTextures() 
{
    for (auto& pair : textures) 
    {
        vkDestroyImageView(device->getLogicalDevice(), pair.second.imageView, nullptr);
        vkDestroyImage(device->getLogicalDevice(), pair.second.image, nullptr);
        vkFreeMemory(device->getLogicalDevice(), pair.second.memory, nullptr);
    }
    textures.clear();
}
void Renderer::cleanupSwapchain() 
{
    // Destroy framebuffers for each swapchain image view
    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(device->getLogicalDevice(), framebuffer, nullptr);
    }
    framebuffers.clear();

    // Destroy the graphics pipeline and pipeline layout, as they may depend on swapchain format or extent
    if (graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device->getLogicalDevice(), graphicsPipeline, nullptr);
        graphicsPipeline = VK_NULL_HANDLE;
    }

    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device->getLogicalDevice(), pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    // Destroy render pass
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device->getLogicalDevice(), renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }

    // Free command buffers, as they are tied to the old swapchain images
    if (!commandBuffers.empty()) {
        vkFreeCommandBuffers(device->getLogicalDevice(), device->getCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        commandBuffers.clear();
    }

    // Call the swapchain�s own cleanup function to destroy swapchain and associated image views
    if (swapchain != nullptr) {
        swapchain->cleanup();
    }
}


VkCommandBuffer Renderer::getCurrentCommandBuffer() const
{
    return commandBuffers[currentFrame];
}

// Create the render pass
void Renderer::createRenderPass()
{
    // subpasses
    std::array<VkSubpassDescription, 2> subpasses = {};

    // SUBPASS 1 attachments, input attachments and reference
    // Color attachment (Input)
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = findColorFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // depth attachment (Input)
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Color attachment input reference to subpass
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 1; // The index of the color attachment
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment input reference
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 2; // index of the depth attachment
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Setting up subpass 1.
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = 1;
    subpasses[0].pColorAttachments = &colorAttachmentRef;
    subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;

    // SUBPASS 2 attachments and references.
    // Swapchain color attachment.
    VkAttachmentDescription swapchainColorAttachment{};
    swapchainColorAttachment.format = swapchain->getImageFormat();               // Format of the swapchain image
    swapchainColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                    // Number of samples to write for multisampling
    swapchainColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;               // Clear the attachment at the beginning
    swapchainColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;             // Store the result of the rendering
    swapchainColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    swapchainColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    swapchainColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;      // The swapchain image is ready for presentation

    // Swapchain color attachment reference to subpass
    VkAttachmentReference swapchainColorAttachmentRef{};
    swapchainColorAttachmentRef.attachment = 0; // The index of the color attachment
    swapchainColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // references to the attachments that subpass will take input from.
    std::array<VkAttachmentReference, 2> inputReferences;
    inputReferences[0].attachment = 1;
    inputReferences[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputReferences[1].attachment = 2;
    inputReferences[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // setting up subpass 2.
    subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[1].colorAttachmentCount = 1;
    subpasses[1].pColorAttachments = &swapchainColorAttachmentRef;
    subpasses[1].inputAttachmentCount = static_cast<uint32_t>(inputReferences.size());
    subpasses[1].pInputAttachments = inputReferences.data();

    // SUBPASS DEPENDENCIES
    // determine when layout transitions occur using subpass dependencies
    std::array<VkSubpassDependency, 3> subpassDependencies;

    // conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = 0;

    // Subpass 1 layout (color/depth) to subpass 2 layout (shader read)
    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[1].dstSubpass = 1;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    subpassDependencies[1].dependencyFlags = 0;

    // conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    subpassDependencies[2].srcSubpass = 0;
    subpassDependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[2].dependencyFlags = 0;

    // Create render pass info.
    std::array<VkAttachmentDescription, 3> attachments = { swapchainColorAttachment, colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderPassInfo.pDependencies = subpassDependencies.data();

    if (vkCreateRenderPass(device->getLogicalDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
}

void Renderer::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding vpLayoutBinding{};
    vpLayoutBinding.binding = 0; // Binding point in shader, the binding number in shader.
    vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpLayoutBinding.descriptorCount = 1;
    vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    vpLayoutBinding.pImmutableSamplers = nullptr;

    // Model binding info
    /* need to define with uniform dynamic buffer not in use with push constants.
    VkDescriptorSetLayoutBinding modelLayoutBinding = {};
    modelLayoutBinding.binding = 1;
    modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    modelLayoutBinding.descriptorCount = 1;
    modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    modelLayoutBinding.pImmutableSamplers = nullptr;

    std::vector< VkDescriptorSetLayoutBinding> layoutBindings = { vpLayoutBinding, modelLayoutBinding };
    */
    std::vector< VkDescriptorSetLayoutBinding> layoutBindings = { vpLayoutBinding };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutInfo.pBindings = layoutBindings.data();

    if (vkCreateDescriptorSetLayout(device->getLogicalDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    // Create texture sampler descriptor set layout
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo = {};
    textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutCreateInfo.bindingCount = 1;
    textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(device->getLogicalDevice(), &textureLayoutCreateInfo, nullptr, &samplerSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a sampler descriptor set layout!");
    }

    // input attachment image descriptor set layout
    VkDescriptorSetLayoutBinding colorInputLayoutBinding = {};
    colorInputLayoutBinding.binding = 0;
    colorInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    colorInputLayoutBinding.descriptorCount = 1;
    colorInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // depth input binding
    VkDescriptorSetLayoutBinding depthInputLayoutBinding = {};
    depthInputLayoutBinding.binding = 1;
    depthInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    depthInputLayoutBinding.descriptorCount = 1;
    depthInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> inputBindings = { colorInputLayoutBinding, depthInputLayoutBinding };
    VkDescriptorSetLayoutCreateInfo inputLayoutCreateInfo = {};
    inputLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    inputLayoutCreateInfo.bindingCount = static_cast<uint32_t>(inputBindings.size());
    inputLayoutCreateInfo.pBindings = inputBindings.data();

    if (vkCreateDescriptorSetLayout(device->getLogicalDevice(), &inputLayoutCreateInfo, nullptr, &inputSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a input descriptor set layout!");
    }

}

void Renderer::createPushConstantRange()
{
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(Model);
}


// Create the pipeline
void Renderer::createGraphicsPipeline() 
{
    // Shader stages.
    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0] = createShaderStage("shaders/vertex_shader.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = createShaderStage("shaders/fragment_shader.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    // Vertex input state
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    std::vector<VkVertexInputBindingDescription> bindingDescriptions = Vertex::getBindingDescriptions();
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor state
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchain->getExtent().width;
    viewport.height = (float)swapchain->getExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchain->getExtent();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterizer state
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisample state
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;               // enabling multisampling or not
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // number of samples to use per fragment

    // Color blend state
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_TRUE;
    // (srcColorBlendFactor * new colour) colorBlendOp (dstColorBlendFactor * old color)
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // Pipeline layout
    std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = { descriptorSetLayout  , samplerSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    // Add descriptor set layouts if you have any
    if (vkCreatePipelineLayout(device->getLogicalDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }


    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
    depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCreateInfo.depthTestEnable = VK_TRUE;             // Enable depth testing
    depthStencilCreateInfo.depthWriteEnable = VK_TRUE;            // Enable depth writing
    depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;   // Allows and allows overwrite in front
    depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2; // Number of shader stages
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencilCreateInfo; // enable depth test
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass; // Ensure renderPass is created and not null
    pipelineInfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);
    if (result != VK_SUCCESS) 
    {
        std::cerr << "Failed to create graphics pipeline! Error code: " << result << std::endl;
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    // SECOND PASS PIPELINE
    VkPipelineShaderStageCreateInfo secondPassShaderStages[2];
    secondPassShaderStages[0] = createShaderStage("shaders/second_pass_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    secondPassShaderStages[1] = createShaderStage("shaders/second_pass_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    depthStencilCreateInfo.depthWriteEnable = VK_FALSE;
    
    VkPipelineLayoutCreateInfo secondPipelineLayoutCreateInfo = {};
    secondPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    secondPipelineLayoutCreateInfo.setLayoutCount = 1;
    secondPipelineLayoutCreateInfo.pSetLayouts = &inputSetLayout;
    secondPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    secondPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    result = vkCreatePipelineLayout(device->getLogicalDevice(), &secondPipelineLayoutCreateInfo, nullptr, &secondPipelineLayout);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create second pipeline layout!");
    }

    pipelineInfo.pStages = secondPassShaderStages;
    pipelineInfo.layout = secondPipelineLayout;
    pipelineInfo.subpass = 1;

    // create second pass pipeline
    result = vkCreateGraphicsPipelines(device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &secondPipeline);
    if (result != VK_SUCCESS)
    {
        std::cerr << "Failed to create graphics pipeline! Error code: " << result << std::endl;
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
}

void Renderer::createColorBufferImage()
{
    colorBufferImages.resize(swapchain->getImageCount());
    colorBufferImageMemory.resize(swapchain->getImageCount());
    colorBufferImageViews.resize(swapchain->getImageCount());

    VkFormat colorFormat = findColorFormat();

    for (size_t i = 0; i < swapchain->getImageCount(); i++)
    {
        createImage(swapchain->getExtent().width,
                    swapchain->getExtent().height,
                    colorFormat,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    &colorBufferImages[i],
                    &colorBufferImageMemory[i]);

        colorBufferImageViews[i] = createImageView(colorBufferImages[i], colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void Renderer::createDepthBufferImage()
{

    depthBufferImages.resize(swapchain->getImageCount());
    depthBufferImageMemory.resize(swapchain->getImageCount());
    depthBufferImageViews.resize(swapchain->getImageCount());

    VkFormat depthFormat = findDepthFormat();

    for (size_t i = 0; i < swapchain->getImageCount(); i++)
    {
        createImage(swapchain->getExtent().width, swapchain->getExtent().height,
            depthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &depthBufferImages[i],
            &depthBufferImageMemory[i]);
        depthBufferImageViews[i] = createImageView(depthBufferImages[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
}

// Create framebuffers for each swapchain image view
void Renderer::createFramebuffers() 
{
    framebuffers.resize(swapchain->getImageCount());

    for (size_t i = 0; i < framebuffers.size(); i++) 
    {
        std::array<VkImageView, 3> attachments = {  swapchain->getImageView(i),
                                                    colorBufferImageViews[i],
                                                    depthBufferImageViews[i]
                                                 };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchain->getExtent().width;
        framebufferInfo.height = swapchain->getExtent().height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device->getLogicalDevice(), &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) 
        {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}

void Renderer::createCommandBuffers() 
{
    commandBuffers.resize(framebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = device->getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device->getLogicalDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

void Renderer::createTextureSampler() {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device->getPhysicalDevice(), &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.mipLodBias = 0.0f;

    // Allocate memory for sampler
    
    if (vkCreateSampler(device->getLogicalDevice(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler!");
    }

}


// Create semaphores and fences for frame synchronization
void Renderer::createSyncObjects()
{
    // Ensure the vectors have the correct size
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    // Create semaphores and fences for each frame
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device->getLogicalDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device->getLogicalDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device->getLogicalDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects for a frame!");
        }
    }
}

void Renderer::createDescriptorPools()
{
    // create uniform descriptor pool

    // ViewProjection pool
    VkDescriptorPoolSize vpPoolSize = {};
    vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffers.size());

    // Model pool dynamic
    /* used with model dynamic buffers
    VkDescriptorPoolSize modelPoolSize = {};
    modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    modelPoolSize.descriptorCount = static_cast<uint32_t>(modelDynUniformBuffers.size());

    std::vector<VkDescriptorPoolSize> descriptorPoolSizes = { vpPoolSize, modelPoolSize };
*/
    std::vector<VkDescriptorPoolSize> descriptorPoolSizes = { vpPoolSize };
    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = static_cast<uint32_t>(swapchain->getImageCount());
    poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
    poolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
    
    VkResult result = vkCreateDescriptorPool(device->getLogicalDevice(), &poolCreateInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a descriptor pool!");
    }

    // create texture sampler pool
    VkDescriptorPoolSize samplerPoolSize = {};
    samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerPoolSize.descriptorCount = MAX_OBJECTS;
    
    VkDescriptorPoolCreateInfo samplerPoolCreateInfo = {};
    samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    samplerPoolCreateInfo.maxSets = MAX_OBJECTS;
    samplerPoolCreateInfo.poolSizeCount = 1;
    samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

    result = vkCreateDescriptorPool(device->getLogicalDevice(), &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a sampler descriptor pool!");
    }

    // create input attachment descriptor pool
    VkDescriptorPoolSize colorInputPoolSize = {};
    colorInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    colorInputPoolSize.descriptorCount = static_cast<uint32_t>(colorBufferImageViews.size());

    VkDescriptorPoolSize depthInputPoolSize = {};
    depthInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    depthInputPoolSize.descriptorCount = static_cast<uint32_t>(depthBufferImageViews.size());

    std::vector<VkDescriptorPoolSize> inputPoolSizes = { colorInputPoolSize, depthInputPoolSize };

    VkDescriptorPoolCreateInfo inputPoolCreateInfo = {};
    inputPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    inputPoolCreateInfo.maxSets = swapchain->getImageCount();
    inputPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(inputPoolSizes.size());
    inputPoolCreateInfo.pPoolSizes = inputPoolSizes.data();
    result = vkCreateDescriptorPool(device->getLogicalDevice(), &inputPoolCreateInfo, nullptr, &inputDescriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a input descriptor pool!");
    }
}

void Renderer::createDescriptorSets()
{
    descriptorSets.resize(swapchain->getImageCount());

    std::vector<VkDescriptorSetLayout> setLayouts(swapchain->getImageCount(), descriptorSetLayout);

    VkDescriptorSetAllocateInfo setAllocInfo = {};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = descriptorPool;
    setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapchain->getImageCount());
    setAllocInfo.pSetLayouts = setLayouts.data();

    // Alocate descriptor sets
    VkResult result = vkAllocateDescriptorSets(device->getLogicalDevice(), &setAllocInfo, descriptorSets.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    // Update all of descriptor set buffer bindings
    for (size_t i = 0; i < swapchain->getImageCount(); ++i)
    {
        // ViewProjection Descriptor.
        VkDescriptorBufferInfo vpBufferInfo = {};
        vpBufferInfo.buffer = vpUniformBuffers[i];
        vpBufferInfo.offset = 0;
        vpBufferInfo.range = sizeof(UboViewProjection);

        VkWriteDescriptorSet vpSetWrite = {};
        vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vpSetWrite.dstSet = descriptorSets[i];
        vpSetWrite.dstBinding = 0;
        vpSetWrite.dstArrayElement = 0;
        vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vpSetWrite.descriptorCount = 1;
        vpSetWrite.pBufferInfo = &vpBufferInfo;

        // Model descriptor.
        /* left for example if dynamic buffer usage, not in use with push constant for model transform
        VkDescriptorBufferInfo modelBufferInfo = {};
        modelBufferInfo.buffer = modelDynUniformBuffers[i];
        modelBufferInfo.offset = 0;
        modelBufferInfo.range = modelUniformAlignment;

        VkWriteDescriptorSet modelSetWrite = {};
        modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        modelSetWrite.dstSet = descriptorSets[i];
        modelSetWrite.dstBinding = 1;
        modelSetWrite.dstArrayElement = 0;
        modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        modelSetWrite.descriptorCount = 1;
        modelSetWrite.pBufferInfo = &modelBufferInfo;
        
        std::vector< VkWriteDescriptorSet> setWrites = { vpSetWrite, modelSetWrite };
        */
        std::vector< VkWriteDescriptorSet> setWrites = { vpSetWrite };

        // update the descriptor sets with new buffer/binding info.
        vkUpdateDescriptorSets(device->getLogicalDevice(),
                                static_cast<uint32_t>(setWrites.size()), 
                                setWrites.data(), 
                                0, nullptr);
    }
}

int Renderer::createTextureDescriptor(VkImageView textureImage)
{
    VkDescriptorSet descriptorSet;
    
    VkDescriptorSetAllocateInfo setAllocateInfo = {};
    setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocateInfo.descriptorPool = samplerDescriptorPool;
    setAllocateInfo.descriptorSetCount = 1;
    setAllocateInfo.pSetLayouts = &samplerSetLayout;

    VkResult result = vkAllocateDescriptorSets(device->getLogicalDevice(), &setAllocateInfo, &descriptorSet);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate texture descriptor set!");
    }

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureImage;
    imageInfo.sampler = textureSampler;

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device->getLogicalDevice(), 1, &descriptorWrite, 0, nullptr);
    
    samplerDescriptorSets.push_back(descriptorSet);

    return samplerDescriptorSets.size() - 1;
}

int Renderer::createMeshModel(std::string modelPath, std::string modelFile)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(modelPath + modelFile, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

    if (!scene)
    {
        throw std::runtime_error("Failed to load model! (" + modelPath + modelFile +")");
    }

    std::vector<std::string> textureNames = MeshModel::loadMaterials(scene);

    // Create textures
    std::vector<int> matToTex(textureNames.size());
    for (size_t i = 0; i < textureNames.size(); ++i)
    {
        if (textureNames[i].empty())
        {
            matToTex[i] = 0;
        }
        else
        {
            Texture* texture = getTexture(modelPath + textureNames[i]);
            matToTex[i] = texture->textId;
        }
    }

    // Load all the meshes
    std::vector<Mesh> modelMeshes = MeshModel::LoadNode(device, scene->mRootNode, scene, matToTex);

    MeshModel meshModel = MeshModel(modelMeshes);
    modelList.push_back(meshModel);
    return modelList.size() - 1;
}

void Renderer::createUniformBuffers()
{
    // viewProjection buffer size.
    VkDeviceSize vpBufferSize = sizeof(UboViewProjection);

    // model buffer size
    //VkDeviceSize modelBufferSize = modelUniformAlignment * MAX_OBJECTS;

    vpUniformBuffers.resize(swapchain->getImageCount());
    vpUniformBuffersMemory.resize(swapchain->getImageCount());

    //modelDynUniformBuffers.resize(swapchain->getImageCount());
    //modelDynUniformBuffersMemory.resize(swapchain->getImageCount());

    for (size_t i = 0; i < swapchain->getImageCount(); ++i)
    {
        createBuffer(device->getLogicalDevice(), device->getPhysicalDevice(), vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            vpUniformBuffers[i], vpUniformBuffersMemory[i]);

        /*
        createBuffer(device->getLogicalDevice(), device->getPhysicalDevice(), modelBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            modelDynUniformBuffers[i], modelDynUniformBuffersMemory[i]);
            */
    }

}

void Renderer::updateUniformBuffers(uint32_t imageIndex)
{
    if (vpUniformBuffersMemory.empty() || imageIndex >= vpUniformBuffersMemory.size()) {
        // Skip updating if no uniform buffers are available or index is out of range
        return;
    }

    // ViewProjection data
    void* data;
    VkResult result = vkMapMemory(device->getLogicalDevice(), vpUniformBuffersMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to map uniform buffer memory!");
    }
    memcpy(data, &uboViewProjection, sizeof(UboViewProjection));
    vkUnmapMemory(device->getLogicalDevice(), vpUniformBuffersMemory[imageIndex]);

    // Model data

    /* this is not in use now, left for example of updating uniform dynamic buffer!
    for (size_t i = 0; i < meshes.size(); ++i)
    {
        Model* model = (Model*)((uint64_t)modelTransferSpace + (i * modelUniformAlignment));
        *model = meshes[i]->getModel();
    }

    result = vkMapMemory(device->getLogicalDevice(), modelDynUniformBuffersMemory[imageIndex], 0, modelUniformAlignment * meshes.size(), 0, &data);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to map model uniform buffer memory!");
    }
    memcpy(data, modelTransferSpace, modelUniformAlignment * meshes.size());
    vkUnmapMemory(device->getLogicalDevice(), modelDynUniformBuffersMemory[imageIndex]);
    */
}


VkPipelineShaderStageCreateInfo Renderer::createShaderStage(const std::string& filepath, VkShaderStageFlagBits stage)
{
    // Read the shader code from file
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filepath);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    // Create shader module
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device->getLogicalDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module: " + filepath);
    }

    // Store the shader module for later cleanup
    shaderModules.push_back(shaderModule); // Add the created shader module to the vector

    // Create shader stage info
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";  // Entry point for the shader

    return shaderStageInfo; // Return the created shader stage info
}

void Renderer::allocateDynamicBufferTransferSpace()
{
    /*
    modelUniformAlignment = (sizeof(Model) + minUniformBufferOffset - 1) & ~(minUniformBufferOffset - 1);

    // create space in memory to hold dynamic buffer aligned to required alignment and hold max objects.
    modelTransferSpace = (Model*)_aligned_malloc(modelUniformAlignment * MAX_OBJECTS, modelUniformAlignment);
    if (!modelTransferSpace) {
        throw std::runtime_error("Failed to allocate aligned memory for modelTransferSpace.");
    }
    */
}

void Renderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkImage* image, VkDeviceMemory* imageMemory)
{

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;                             // Single layer
    imageInfo.mipLevels = 1;                                // number of mipmap levels
    imageInfo.arrayLayers = 1;                              // Single image
    imageInfo.format = format;                              // Format (e.g., depth format)
    imageInfo.tiling = tiling;                              // Tiling (e.g., optimal tiling)
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;    // Initial layout
    imageInfo.usage = usageFlags;                           // Usage flags (e.g., depth-stencil attachment)
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;              // Single sample (no multisampling)
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;      // Exclusive access, wether image can be shared between queues

    // Create the image
    if (vkCreateImage(device->getLogicalDevice(), &imageInfo, nullptr, image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }

    // Get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device->getLogicalDevice(), *image, &memRequirements);

    // Allocate memory
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size; // Required memory size
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, memoryPropertyFlags, device->getPhysicalDevice());

    if (vkAllocateMemory(device->getLogicalDevice(), &allocInfo, nullptr, imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    // Bind memory to the image
    vkBindImageMemory(device->getLogicalDevice(), *image, *imageMemory, 0);

}

VkImageView Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;

    // Define the subresource range (for depth, color, etc.)
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device->getLogicalDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) 
    {
        throw std::runtime_error("Failed to create image view!");
    }

    return imageView;
}

VkFormat Renderer::findDepthFormat()
{
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat Renderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device->getPhysicalDevice(), format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) 
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("Failed to find a supported depth format!");
}

void Renderer::loadTexture(const std::string& filePath, Texture& texture)
{
    loadTextureImage(filePath, texture);
    texture.imageView = createImageView(texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

    int descriptorLoc = createTextureDescriptor(texture.imageView);
    texture.textId = descriptorLoc;
}

void Renderer::loadTextureImage(const std::string& filePath, Texture &texture)
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    VkDeviceSize imageSize = texWidth * texHeight * 4; // RGBA (4 bytes per pixel)

    if (!pixels) 
    {
        throw std::runtime_error("Failed to load texture image!");
    }

    // Create staging buffer and copy image data to it
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device->getLogicalDevice(), device->getPhysicalDevice(), imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device->getLogicalDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device->getLogicalDevice(), stagingBufferMemory);

    stbi_image_free(pixels);

    // Create Vulkan image
    createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texture.image, &texture.memory);

    // Transition image to be ready for data copy
    transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    // Copy buffer to the new created image
    copyBufferToImage(stagingBuffer, texture.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    
    // Transition image for shader sampling
    transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Cleanup staging buffer
    vkDestroyBuffer(device->getLogicalDevice(), stagingBuffer, nullptr);
    vkFreeMemory(device->getLogicalDevice(), stagingBufferMemory, nullptr);
}

void Renderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = beginCommandBuffer(device->getLogicalDevice(), device->getCommandPool());

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;

    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::runtime_error("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0, 0, nullptr, 0, nullptr, 1, &barrier
    );

    endAndSubmitCommandBuffer(device->getLogicalDevice(), device->getGraphicsQueue(), device->getCommandPool(), commandBuffer);
}

void Renderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = beginCommandBuffer(device->getLogicalDevice(), device->getCommandPool());

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    endAndSubmitCommandBuffer(device->getLogicalDevice(), device->getGraphicsQueue(), device->getCommandPool(), commandBuffer);
}

int Renderer::initImGui()
{
    try {
        imguiManager = new ImGuiManager(window->getSDLWindow(),
            instance->getInstance(),
            device->getLogicalDevice(),
            device->getPhysicalDevice(),
            device->getGraphicsQueue(),
            device->getGraphicsQueueFamilyIndex(),
            getRenderPass());

    }
    catch (std::runtime_error& e) {
        Logger::error("Failed to initialize ImGui: " + std::string(e.what()));
        return EXIT_FAILURE;
    }
    return 0;
}

VkFormat Renderer::findColorFormat()
{
    return findSupportedFormat(
        { VK_FORMAT_R8G8B8A8_UNORM },
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );
}

void Renderer::cleanup()
{
    
    //if (modelTransferSpace) {
    //    _aligned_free(modelTransferSpace);
    //    modelTransferSpace = nullptr;
    //}


    if (imguiManager)
    {
        delete imguiManager;
        imguiManager = nullptr;
    }

    if (device)
    {
        vkDeviceWaitIdle(device->getLogicalDevice());

        for (size_t i = 0; i < modelList.size(); ++i)
        {
            modelList[i].destroyMeshModel();
        }

        vkDestroyDescriptorPool(device->getLogicalDevice(), samplerDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device->getLogicalDevice(), samplerSetLayout, nullptr);

        for (size_t i = 0; i < depthBufferImages.size(); i++)
        {
            vkDestroyImageView(device->getLogicalDevice(), depthBufferImageViews[i], nullptr);
            vkDestroyImage(device->getLogicalDevice(), depthBufferImages[i], nullptr);
            vkFreeMemory(device->getLogicalDevice(), depthBufferImageMemory[i], nullptr);
        }

        for (size_t i = 0; i < colorBufferImages.size(); i++)
        {
            vkDestroyImageView(device->getLogicalDevice(), colorBufferImageViews[i], nullptr);
            vkDestroyImage(device->getLogicalDevice(), colorBufferImages[i], nullptr);
            vkFreeMemory(device->getLogicalDevice(), colorBufferImageMemory[i], nullptr);
        }

        cleanupTextures();
        vkDestroySampler(device->getLogicalDevice(), textureSampler, nullptr);

        if (descriptorPool != VK_NULL_HANDLE) 
        {
            Logger::info("Destroying descriptor pool.");
            vkDestroyDescriptorPool(device->getLogicalDevice(), descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }
        else {
            Logger::warning("Descriptor pool is already null.");
        }

        vkDestroyDescriptorPool(device->getLogicalDevice(), inputDescriptorPool, nullptr);
        if (inputSetLayout != VK_NULL_HANDLE)
        {
            Logger::info("Destroying input descriptor set layout.");
            vkDestroyDescriptorSetLayout(device->getLogicalDevice(), inputSetLayout, nullptr);
            inputSetLayout = VK_NULL_HANDLE;
        }

        if (descriptorSetLayout != VK_NULL_HANDLE)
        {
            Logger::info("Destroying descriptor set layout.");
            vkDestroyDescriptorSetLayout(device->getLogicalDevice(), descriptorSetLayout, nullptr);
            descriptorSetLayout = VK_NULL_HANDLE;
        }
        else {
            Logger::warning("Descriptor set layout is already null.");
        }

        for (size_t i = 0; i < vpUniformBuffers.size(); ++i)
        {
            if (vpUniformBuffers[i] != VK_NULL_HANDLE) 
            {
                Logger::info("Destroying uniform buffer at index " + std::to_string(i));
                vkDestroyBuffer(device->getLogicalDevice(), vpUniformBuffers[i], nullptr);
                vpUniformBuffers[i] = VK_NULL_HANDLE;
            }
            else {
                Logger::warning("VP Uniform buffer at index " + std::to_string(i) + " is already null.");
            }
        }

        for (size_t i = 0; i < vpUniformBuffersMemory.size(); ++i) 
        {
            if (vpUniformBuffersMemory[i] != VK_NULL_HANDLE)
            {
                Logger::info("Freeing uniform buffer memory at index " + std::to_string(i));
                vkFreeMemory(device->getLogicalDevice(), vpUniformBuffersMemory[i], nullptr);
                vpUniformBuffersMemory[i] = VK_NULL_HANDLE;
            }
            else {
                Logger::warning("VP Uniform buffer memory at index " + std::to_string(i) + " is already null.");
            }
        }

        for (size_t i = 0; i < modelDynUniformBuffers.size(); ++i)
        {
            if (modelDynUniformBuffers[i] != VK_NULL_HANDLE)
            {
                Logger::info("Destroying uniform buffer at index " + std::to_string(i));
                vkDestroyBuffer(device->getLogicalDevice(), modelDynUniformBuffers[i], nullptr);
                modelDynUniformBuffers[i] = VK_NULL_HANDLE;
            }
            else {
                Logger::warning("Model dynamic Uniform buffer at index " + std::to_string(i) + " is already null.");
            }
        }

        for (size_t i = 0; i < modelDynUniformBuffersMemory.size(); ++i)
        {
            if (modelDynUniformBuffersMemory[i] != VK_NULL_HANDLE)
            {
                Logger::info("Freeing uniform buffer memory at index " + std::to_string(i));
                vkFreeMemory(device->getLogicalDevice(), modelDynUniformBuffersMemory[i], nullptr);
                modelDynUniformBuffersMemory[i] = VK_NULL_HANDLE;
            }
            else {
                Logger::warning("Model Dynamic Uniform buffer memory at index " + std::to_string(i) + " is already null.");
            }
        }

        // Destroy graphics pipelines

        if (secondPipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device->getLogicalDevice(), secondPipeline, nullptr);
            secondPipeline = VK_NULL_HANDLE;
        }

        // Destroy pipeline layout
        if (secondPipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device->getLogicalDevice(), secondPipelineLayout, nullptr);
            secondPipelineLayout = VK_NULL_HANDLE;
        }

        if (graphicsPipeline != VK_NULL_HANDLE) 
        {
            vkDestroyPipeline(device->getLogicalDevice(), graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
        }

        // Destroy pipeline layout
        if (pipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device->getLogicalDevice(), pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }

        // Destroy render pass
        if (renderPass != VK_NULL_HANDLE) 
        {
            vkDestroyRenderPass(device->getLogicalDevice(), renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }

        // Destroy framebuffers
        for (auto framebuffer : framebuffers) 
        {
            if (framebuffer != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(device->getLogicalDevice(), framebuffer, nullptr);
            }
        }
        framebuffers.clear();

        // Cleanup synchronization objects (semaphores and fences)
        for (size_t i = 0; i < inFlightFences.size(); i++) 
        {
            if (renderFinishedSemaphores[i] != VK_NULL_HANDLE) 
            {
                vkDestroySemaphore(device->getLogicalDevice(), renderFinishedSemaphores[i], nullptr);
            }
            if (imageAvailableSemaphores[i] != VK_NULL_HANDLE) 
            {
                vkDestroySemaphore(device->getLogicalDevice(), imageAvailableSemaphores[i], nullptr);
            }
            if (inFlightFences[i] != VK_NULL_HANDLE) 
            {
                vkDestroyFence(device->getLogicalDevice(), inFlightFences[i], nullptr);
            }
        }
        renderFinishedSemaphores.clear();
        imageAvailableSemaphores.clear();
        inFlightFences.clear();

        // Destroy shader modules
        for (auto shaderModule : shaderModules)
        {
            if (shaderModule != VK_NULL_HANDLE) 
            {
                vkDestroyShaderModule(device->getLogicalDevice(), shaderModule, nullptr);
            }
        }
        shaderModules.clear();

        this->device = nullptr;
    }
}
