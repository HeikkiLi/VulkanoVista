#pragma once


#define SDL_MAIN_HANDLED

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <string>


#include "Instance.h"

class Window 
{
public:
    ~Window();

    void create(int width, int height, const std::string& title);   // Create the window
    void createSurface(const Instance& instance);                   // Create Vulkan surface
    bool shouldClose() const; // Check if window should close
    void pollEvents(); // Poll for window events
    VkSurfaceKHR getSurface() const; // Get Vulkan surface
    VkExtent2D getExtent() const;
    void cleanup(); // Cleanup resources

    SDL_Window* getSDLWindow() { return sdlWindow; }

private:
    SDL_Window* sdlWindow;

    //  The surface represents the window or screen that will display the rendered image.
    VkSurfaceKHR surface; 

    bool isClosed = false;
};
