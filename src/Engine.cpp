#include "Engine.h"


void Engine::init()
{
    initWindow();
    initVulkan();
}

void Engine::run()
{
    while (!window.shouldClose()) 
    {
        window.pollEvents();
        renderer.drawFrame();
    }
    device.waitIdle();
}

void Engine::cleanup()
{
    renderer.cleanup();
    swapchain.cleanup();
    device.cleanup();
    instance.cleanup();
    window.cleanup();
}

void Engine::initWindow()
{
	window.create(WIDTH, HEIGHT, "VulkanoVista");
}

void Engine::initVulkan()
{
    instance.create(window.getSDLWindow());
    window.createSurface(instance);
    instance.SetSurface(window.getSurface());

    device.pickPhysicalDevice(instance, window.getSurface());
    device.createLogicalDevice(window.getSurface());
    swapchain.create(device, window.getSurface(), WIDTH, HEIGHT);
    renderer.setup(device, swapchain);
}
