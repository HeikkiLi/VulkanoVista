#pragma once

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>

const int MAX_OBJECTS = 2;

// Function to find a suitable memory type index
uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice);

// Function to create a Vulkan buffer
void createBuffer(VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory);
