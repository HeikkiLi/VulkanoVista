#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "Device.h"

class Swapchain
{
public:
    void create(const Device& device, VkSurfaceKHR surface, int width, int height);
    void cleanup();

    VkSwapchainKHR getSwapchain() const { return swapchain; }
    VkFormat getImageFormat() const { return imageFormat; }
    VkExtent2D getExtent() const { return extent; }
    const std::vector<VkImageView>& getImageViews() const { return imageViews; }

    uint32_t getImageCount() const;            // Returns the number of images in the swapchain
    VkImageView getImageView(size_t index) const; // Returns the image view at a specified index

private:
    // Helper functions for configuring the swapchain
    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int width, int height);

    // Swapchain handle and properties
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat imageFormat;                   // Format of swapchain images
    VkExtent2D extent;                      // Dimensions of swapchain images
    std::vector<VkImage> images;            // Images in the swapchain
    std::vector<VkImageView> imageViews;    // Image views for each swapchain image

    // Reference to the logical device (for cleanup, etc.)
    const Device* device = nullptr;
};

