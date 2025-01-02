#include "Device.h"
#include "Logger.h"


void Device::pickPhysicalDevice(const Instance& instance, VkSurfaceKHR surface) 
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance.getInstance(), &deviceCount, nullptr);

    if (deviceCount == 0) 
    {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance.getInstance(), &deviceCount, devices.data());

    for (const auto& device : devices) 
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        Logger::info("Device Name: " + std::string(deviceProperties.deviceName));
        Logger::info("Device Type: " + std::to_string(static_cast<int>(deviceProperties.deviceType)));

        if (isDeviceSuitable(device, surface)) 
        {
            physicalDevice = device;
            Logger::info("Selected Device : " + std::string(deviceProperties.deviceName));
            graphicsQueueFamilyIndex = findQueueFamilies(physicalDevice, surface).graphicsFamily.value();
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) 
    {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

VkPhysicalDevice Device::getPhysicalDevice() const
{
    return physicalDevice;
}

bool Device::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    // Check for graphics queue family support
    QueueFamilyIndices indices = findQueueFamilies(device, surface);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    // Prefer a discrete GPU, though an integrated GPU will also work
    bool isDiscreteGPU = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    return indices.isComplete() && extensionsSupported && swapChainAdequate && isDiscreteGPU && deviceFeatures.geometryShader;
}

QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) 
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport) 
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) 
        {
            break;
        }
        i++;
    }

    return indices;
}

bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) 
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }


    return requiredExtensions.empty();
}

SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) 
{
    SwapChainSupportDetails details;

    // Query surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    // Query surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    // Query presentation modes (use four arguments here)
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) 
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}


bool Device::isSwapchainExtensionSupported(VkPhysicalDevice physicalDevice) 
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    for (const auto& extension : availableExtensions) 
    {
        if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        {
            return true; // Found the VK_KHR_SWAPCHAIN extension
        }
    }
    return false; // Extension not found
}

bool Device::findGraphicsAndPresentQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t& graphicsQueueFamilyIndex, uint32_t& presentQueueFamilyIndex) 
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            graphicsQueueFamilyIndex = i;

            VkBool32 presentSupport = VK_FALSE;
            VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if (result != VK_SUCCESS) 
            {
                std::cerr << "Failed to check surface support for queue family!" << std::endl;
                return false;
            }

            if (presentSupport == VK_TRUE)
            {
                presentQueueFamilyIndex = i;
                return true; // Found a queue family that supports both graphics and presentation
            }
        }
    }

    return false; // No suitable queue family found
}

void Device::createLogicalDevice(VkSurfaceKHR surface)
{
    // Check if the physical device supports swapchain extension
    if (!isSwapchainExtensionSupported(physicalDevice))
    {
        std::cerr << "VK_KHR_SWAPCHAIN extension not supported by the device!" << std::endl;
        throw std::runtime_error("VK_KHR_SWAPCHAIN extension not supported by the device!");
    }

    // Find queue families that support both graphics and presentation
    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    uint32_t presentQueueFamilyIndex = UINT32_MAX;

    if (!findGraphicsAndPresentQueueFamilies(physicalDevice, surface, graphicsQueueFamilyIndex, presentQueueFamilyIndex)) 
    {
        std::cerr << "No queue families found that support both graphics and presentation!" << std::endl;
        throw std::runtime_error("No suitable queue families found!");
    }

    // Queue create info
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // Enabled extensions
    std::vector<const char*> enabledExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    // physical device features logical device will use.
    VkPhysicalDeviceFeatures deviceFeatures = {};
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();

    // Create the logical device
    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        std::cerr << "Failed to create logical device!" << std::endl;
        throw std::runtime_error("Failed to create logical device!");
    }

    // Get the queues for graphics and presentation
    vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentQueueFamilyIndex, 0, &presentQueue);
}

VkDevice Device::getLogicalDevice() const
{
	return device;
}

uint32_t Device::getGraphicsQueueFamilyIndex() const 
{
    return graphicsQueueFamilyIndex;
}

VkQueue Device::getGraphicsQueue() const
{
	return graphicsQueue;
}

VkQueue Device::getPresentQueue() const
{
	return presentQueue;
}

void Device::cleanup()
{

    // Destroy command pool
    if (commandPool != VK_NULL_HANDLE) 
    {
        vkDestroyCommandPool(device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }

	vkDestroyDevice(device, nullptr);
}

void Device::waitIdle()
{
	vkDeviceWaitIdle(device);
}

std::vector<VkSurfaceFormatKHR> Device::getSurfaceFormats(VkSurfaceKHR surface) const 
{
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

    if (formatCount == 0)
    {
        throw std::runtime_error("Failed to find any surface formats!");
    }

    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats.data());

    return surfaceFormats;
}

std::vector<VkPresentModeKHR> Device::getPresentModes(VkSurfaceKHR surface) const
{
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

    if (presentModeCount == 0) 
    {
        throw std::runtime_error("Failed to find any present modes!");
    }

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

    return presentModes;
}

void Device::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) 
{
    // Allocate a temporary command buffer for the copy operation
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) 
    {
        throw std::runtime_error("Failed to allocate command buffer!");
    }

    // Begin recording the command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) 
    {
        throw std::runtime_error("Failed to begin command buffer!");
    }

    // Copy data from the source buffer to the destination buffer
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) 
    {
        throw std::runtime_error("Failed to record command buffer!");
    }

    // Create a fence to wait for the copy operation to complete
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence copyFence;
    if (vkCreateFence(device, &fenceInfo, nullptr, &copyFence) != VK_SUCCESS) 
    {
        throw std::runtime_error("Failed to create fence!");
    }

    // Submit the command buffer to the graphics queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(getGraphicsQueue(), 1, &submitInfo, copyFence) != VK_SUCCESS) 
    {
        throw std::runtime_error("Failed to submit copy command buffer!");
    }

    // Wait for the copy operation to complete
    vkWaitForFences(device, 1, &copyFence, VK_TRUE, UINT64_MAX);

    // Clean up the fence and command buffer
    vkDestroyFence(device, copyFence, nullptr);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}


void Device::createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = getGraphicsQueueFamilyIndex();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool!");
    }
}

VkCommandPool Device::getCommandPool()
{
    return commandPool;
}
