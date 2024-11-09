#include "Engine.h"


void Engine::init()
{
    initWindow();
    initVulkan();
}

void Engine::run() {
    while (!window.shouldClose()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                VkExtent2D newExtent = window.getExtent();
                renderer.recreateSwapchain(newExtent);
            }
            window.pollEvents();
        }

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
    windowExtent = { WIDTH, HEIGHT };
}

void Engine::initVulkan()
{
    instance.create(window.getSDLWindow());
    window.createSurface(instance);
    instance.SetSurface(window.getSurface());

    device.pickPhysicalDevice(instance, window.getSurface());
    device.createLogicalDevice(window.getSurface());
    swapchain.create(&device, window.getSurface(), windowExtent);
    renderer.setup(&device, &swapchain, &window);
}
