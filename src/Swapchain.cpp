#include "Swapchain.h"


void Swapchain::create(Device* device, VkSurfaceKHR surface, VkExtent2D windowExtent) {
    this->device = device;

    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->getPhysicalDevice(), surface, &capabilities);

    // Select surface format, present mode, and swap extent
    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(device->getSurfaceFormats(surface));
    VkPresentModeKHR presentMode = choosePresentMode(device->getPresentModes(surface));
    extent = chooseSwapExtent(capabilities, windowExtent.width, windowExtent.height);

    // Set the image count (minimum plus one for double-buffering, capped by max)
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // Define swapchain create info
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Define how to handle swapchain images (e.g., in case of multiple queues)
    QueueFamilyIndices indices = device->findQueueFamilies(device->getPhysicalDevice(), surface);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    // Create the swapchain
    VkResult result = vkCreateSwapchainKHR(device->getLogicalDevice(), &createInfo, nullptr, &swapchain);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create swapchain! Error code: " << result << std::endl;
        throw std::runtime_error("Failed to create swapchain!");
    }

    // Retrieve swapchain images
    vkGetSwapchainImagesKHR(device->getLogicalDevice(), swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device->getLogicalDevice(), swapchain, &imageCount, swapchainImages.data());
    imageFormat = surfaceFormat.format;

    // Create image views for each image in the swapchain
    swapchainImageViews.resize(swapchainImages.size());
    for (size_t i = 0; i < swapchainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = imageFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device->getLogicalDevice(), &viewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views!");
        }
    }
}

void Swapchain::cleanup() {
    // Destroy image views for each swapchain image
    for (auto imageView : swapchainImageViews) {
        vkDestroyImageView(device->getLogicalDevice(), imageView, nullptr);
    }
    swapchainImageViews.clear();

    // Destroy the swapchain
    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device->getLogicalDevice(), swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}


uint32_t Swapchain::getImageCount() const
{
    return static_cast<uint32_t>(swapchainImages.size());
}

VkImageView Swapchain::getImageView(size_t index) const
{
    if (index >= swapchainImageViews.size()) {
        throw std::out_of_range("Requested image view index is out of range.");
    }
    return swapchainImageViews[index];
}

// Helper to choose the best surface format from available options
VkSurfaceFormatKHR Swapchain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

// Helper to choose the best present mode
VkPresentModeKHR Swapchain::choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; // Guaranteed to be available
}

// Helper to choose the appropriate swap extent based on window and surface capabilities
VkExtent2D Swapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int width, int height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        actualExtent.width = std::max(capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, actualExtent.height));
        return actualExtent;
    }
}
