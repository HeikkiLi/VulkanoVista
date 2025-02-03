#pragma once

#include <vulkan/vulkan.h>
#include <string>

struct Texture {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView imageView;
    int textId;
};
