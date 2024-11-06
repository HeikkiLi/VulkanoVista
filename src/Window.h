#pragma once


#define SDL_MAIN_HANDLED

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>
#include <string>


#include "Instance.h"

class Window 
{
public:
    ~Window();

    void create(int width, int height, const std::string& title);  // Create the window
    void createSurface(const Instance& instance); // Create Vulkan surface
    bool shouldClose() const; // Check if window should close
    void pollEvents(); // Poll for window events
    VkSurfaceKHR getSurface() const; // Get Vulkan surface
    void cleanup(); // Cleanup resources

    SDL_Window* getSDLWindow() { return window; }

private:
    SDL_Window* window;
    VkSurfaceKHR surface;
    bool isClosed = false;
};
