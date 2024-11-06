#include "Instance.h"
#include "Logger.h"

std::vector<const char*> Instance::getRequiredExtensions(SDL_Window* window) {
    unsigned extensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr)) {
        Logger::error("Could not get the number of required instance extensions from SDL.");
        throw std::runtime_error("Failed to get SDL Vulkan extensions.");
    }

    std::vector<const char*> extensions(extensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data())) {
        Logger::error("Could not get the names of required instance extensions from SDL.");
        throw std::runtime_error("Failed to get SDL Vulkan extensions.");
    }
    return extensions;
}

std::vector<const char*> Instance::getValidationLayers() {
    std::vector<const char*> layers;
#if defined(_DEBUG)
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif
    return layers;
}

void Instance::create(SDL_Window* window)
{
    Logger::info("Creating Vulkan instance...");

    // Get required extensions from SDL
    auto extensions = getRequiredExtensions(window);

    // Set validation layers if in debug mode
    auto layers = getValidationLayers();

    // Application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanoVista";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VulkanoVista Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // Instance creation info
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    if (vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS) {
        Logger::error("Failed to create Vulkan instance");
        throw std::runtime_error("Could not create Vulkan instance.");
    }
   
}

void Instance::cleanup()
{
    if (vkInstance) 
    { 
        if (vkSurface)
        {
            vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
            vkSurface = VK_NULL_HANDLE;
        }
        vkDestroyInstance(vkInstance, nullptr);
       
        vkInstance = VK_NULL_HANDLE;
        Logger::info("Vulkan Instance destroyed.");
    }
}
