#pragma once

#include <vulkan/vulkan.h>

class SyncObjects 
{
public:
    void createSyncObjects(); // Create synchronization objects
    void cleanup(); // Cleanup synchronization objects

private:
    VkSemaphore imageAvailableSemaphore; // Semaphore for image availability
    VkSemaphore renderFinishedSemaphore; // Semaphore for rendering completion
    VkFence inFlightFence; // Fence for frame synchronization

};
