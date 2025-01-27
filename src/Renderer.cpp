#include "Renderer.h"

#include <stdexcept>
#include <fstream>
#include <vector>
#include <array>

#include <glm/gtc/matrix_transform.hpp>

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

void Renderer::setup(Device* device,  Swapchain* swapchain, Window* window)
{
    this->device = device;          // Store pointer to Device
    this->swapchain = swapchain;    // Store pointer to Swapchain
    this->window = window;          // and window..

    device->createCommandPool();

    createRenderPass();
    createPushConstantRange();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createFramebuffers();

    createCommandBuffers();

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
    uboViewProjection.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    uboViewProjection.projection[1][1] *= -1;

    createUniformBuffers();

}

void Renderer::finalizeSetup()
{
    createDescriptorPool();
    createDescriptorSets();
}

// Acquire image from swapchain, record command buffer, submit, and present
void Renderer::drawFrame()
{
    // wait for fences to signal (open) from last draw
    vkWaitForFences(device->getLogicalDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    // close fences
    vkResetFences(device->getLogicalDevice(), 1, &inFlightFences[currentFrame]);


    // Acquire an image from the swapchain
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device->getLogicalDevice(), swapchain->getSwapchain(), UINT64_MAX,
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(window->getExtent());  // If the swapchain is outdated, recreate it.
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    updateUniformBuffers(imageIndex);

    // Record commands to command buffer (for this specific image)
    vkResetCommandBuffer(commandBuffers[imageIndex], 0);
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
    const float rotationSpeed = 45.0f; // Rotate 45 degrees per second

    // Calculate rotation angle in radians based on deltaTime
    float rotationAngle = glm::radians(rotationSpeed * deltaTime);

    for (size_t i = 0; i < meshes.size(); i++)
    {
        glm::mat4 model = meshes[i]->getModel().model;

        float direction = (i == 0) ? -1.0f : 1.0f;
        model = glm::rotate(model, rotationAngle * direction, glm::vec3(0.0f, 0.0f, 1.0f));

        meshes[i]->setModelTransform(model);
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

    VkClearValue clearColor = { {0.0f, 0.0f, 0.0f, 1.0f} };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind graphics pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    
    // Iterate over all meshes and draw them
    for (size_t i = 0; i < meshes.size(); i++) 
    {

        // Bind mesh index and vertex buffers       
        meshes[i]->bind(commandBuffer);

        // Dynamic offset amount
        //uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * i;

        // push constants to given shader.
        Model model = meshes[i]->getModel();
        vkCmdPushConstants(commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(Model),
            &model);

        // bind descriptor sets
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
            0, 1, &descriptorSets[i], 0, nullptr);


        meshes[i]->draw(commandBuffer);  // Issue indexed draw call
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to record command buffer!");
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

void Renderer::addMesh(std::shared_ptr<Mesh> mesh) 
{
    meshes.push_back(mesh);
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


// Create the render pass
void Renderer::createRenderPass() 
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain->getImageFormat();               // Format of the swapchain image
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                    // Number of samples to write for multisampling
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;               // Clear the attachment at the beginning
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;             // Store the result of the rendering
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;      // The swapchain image is ready for presentation

    // Color attachment reference to subpass
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0; // The index of the color attachment
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Configuring the render pass to use this color attachment
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // determine when layout transitions occur using subpass dependencies
    std::array<VkSubpassDependency, 2> subpassDependencies;

    // conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = 0;

    // conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[1].dependencyFlags = 0;

    // Crete render pass info.
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
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
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    // Add descriptor set layouts if you have any
    if (vkCreatePipelineLayout(device->getLogicalDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }


    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;            // Disable depth testing
    depthStencil.depthWriteEnable = VK_FALSE;           // Disable depth writing
    depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS; // Depth comparison is always true
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2; // Number of shader stages
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    //pipelineInfo.pDepthStencilState = &depthStencil; // disable depth test
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass; // Ensure renderPass is created and not null
    pipelineInfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline! Error code: " << result << std::endl;
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
}

// Create framebuffers for each swapchain image view
void Renderer::createFramebuffers() 
{
    framebuffers.resize(swapchain->getImageCount());

    for (size_t i = 0; i < framebuffers.size(); i++) {
        VkImageView attachments[] = { swapchain->getImageView(i) };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchain->getExtent().width;
        framebufferInfo.height = swapchain->getExtent().height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device->getLogicalDevice(), &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
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

void Renderer::createDescriptorPool()
{
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



void Renderer::cleanup()
{
    
    //if (modelTransferSpace) {
    //    _aligned_free(modelTransferSpace);
    //    modelTransferSpace = nullptr;
    //}

    if (device)
    {
        if (descriptorPool != VK_NULL_HANDLE) 
        {
            Logger::info("Destroying descriptor pool.");
            vkDestroyDescriptorPool(device->getLogicalDevice(), descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }
        else {
            Logger::warning("Descriptor pool is already null.");
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

        // Destroy graphics pipeline
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

        for (auto& mesh : meshes) 
        {
            mesh.reset();
        }
        meshes.clear();

        this->device = nullptr;
    }
}
