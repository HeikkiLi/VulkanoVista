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
    void createLogicalDevice(VkSurfaceKHR surface);
    VkDevice getLogicalDevice() const;
    uint32_t getGraphicsQueueFamilyIndex() const;
    VkQueue getGraphicsQueue() const;
    VkQueue getPresentQueue() const;
    void cleanup();
    void waitIdle();

private:
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME  // Required for swapchain creation
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
};
