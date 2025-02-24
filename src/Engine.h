#pragma once

#include "Window.h"
#include "Instance.h"
#include "Device.h"
#include "Swapchain.h"
#include "Renderer.h"

const int WIDTH = 1280;
const int HEIGHT = 720;

class Engine 
{
public:
    // Initialize the engine
    int init();

    // Start the main loop
    void run();   

    // Cleanup resources
    void cleanup(); 

private:
    Window window;
    Instance instance;
    Device device;
    Swapchain swapchain;
    Renderer renderer;

    VkExtent2D windowExtent;  // holding the window size

    int initWindow();
    int initVulkan();
};
