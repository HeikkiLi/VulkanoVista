#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <iostream>
#include <vector>
#include <string>
#include <set>

#include "Instance.h"

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const 
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails 
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};


class Device 
{
public:
    void pickPhysicalDevice(const Instance& instance, VkSurfaceKHR surface);
    VkPhysicalDevice getPhysicalDevice() const;
    void createLogicalDevice(VkSurfaceKHR surface);
    VkDevice getLogicalDevice() const;
    uint32_t getGraphicsQueueFamilyIndex() const;
    VkQueue getGraphicsQueue() const;
    VkQueue getPresentQueue() const;
    void cleanup();
    void waitIdle();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

    std::vector<VkSurfaceFormatKHR> getSurfaceFormats(VkSurfaceKHR surface) const;
    std::vector<VkPresentModeKHR> getPresentModes(VkSurfaceKHR surface) const;


    void createCommandPool();
    VkCommandPool getCommandPool();

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
private:
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;

    VkCommandPool commandPool = VK_NULL_HANDLE;

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME  // Required for swapchain creation
    };

    bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
    bool isSwapchainExtensionSupported(VkPhysicalDevice physicalDevice);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    bool  findGraphicsAndPresentQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t& graphicsQueueFamilyIndex, uint32_t& presentQueueFamilyIndex);

};
