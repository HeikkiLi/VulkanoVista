#pragma once

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <string>
#include <fstream>
#include <sstream>


class ImGuiManager
{
public:
    ImGuiManager(SDL_Window* window, VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, uint32_t queueFamily, VkRenderPass renderPass);
    ~ImGuiManager();

    void shutdown();
    void beginFrame();
    void endFrame(VkCommandBuffer commandBuffer);

    // Shader Editor UI
    void initTextEditor();
    void drawShaderEditor();
    void saveShaderToFile(const std::string& filename);
    void loadShaderFromFile(const std::string& filename);

private:
    bool initialized = false;

    SDL_Window* window;
    VkDevice device;
    VkInstance instance;
    VkDescriptorPool descriptorPool;

};

