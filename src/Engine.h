#pragma once

#include "Window.h"
#include "Instance.h"
#include "Device.h"
#include "Swapchain.h"
#include "Renderer.h"

const int WIDTH = 800;
const int HEIGHT = 600;

class Engine 
{
public:
    void init();  // Initialize the engine
    void run();   // Start the main loop
    void cleanup(); // Cleanup resources

private:
    Window window;
    Instance instance;
    Device device;
    Swapchain swapchain;
    Renderer renderer;

    VkExtent2D windowExtent;  // holding the window size

    void initWindow();
    void initVulkan();

};
