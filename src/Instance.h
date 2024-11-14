#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

class Instance
{
public:

    Instance();
    ~Instance();

    void create(SDL_Window* window); // Initialize Vulkan instance with SDL
    void cleanup(); // Cleanup Vulkan resources

    void SetSurface(VkSurfaceKHR surface) { vkSurface = surface; }

    VkInstance getInstance() const
    {
        return vkInstance;
    }

private:
    VkInstance vkInstance = VK_NULL_HANDLE; // Vulkan instance handle
    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;

    // Helper functions for extension and layer setup
    std::vector<const char*> getRequiredExtensions(SDL_Window* window);
    std::vector<const char*> getValidationLayers();
    bool checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
    bool checkValidationLayerSupport(const std::vector<const char*>& validationLayers);

};
