#include "Swapchain.h"


void Swapchain::create(const Device& device, VkSurfaceKHR surface, int width, int height)
{
}

void Swapchain::cleanup() {
    // Destroy image views for each swapchain image
    for (auto imageView : imageViews) {
        vkDestroyImageView(device->getLogicalDevice(), imageView, nullptr);
    }
    imageViews.clear();

    // Destroy the swapchain
    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device->getLogicalDevice(), swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}


VkSurfaceFormatKHR Swapchain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	return VkSurfaceFormatKHR();
}

VkPresentModeKHR Swapchain::choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	return VkPresentModeKHR();
}

VkExtent2D Swapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int width, int height)
{
	return VkExtent2D();
}

uint32_t Swapchain::getImageCount() const
{
    return static_cast<uint32_t>(images.size());
}

VkImageView Swapchain::getImageView(size_t index) const
{
    if (index >= imageViews.size()) {
        throw std::out_of_range("Requested image view index is out of range.");
    }
    return imageViews[index];
}
