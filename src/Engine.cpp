#include "Engine.h"

#include <stdexcept>
#include <chrono>

#include "Logger.h"
#include "Mesh.h"

int Engine::init()
{
    if (initWindow() == EXIT_FAILURE)
    {
        return EXIT_FAILURE;
    }

    if (initVulkan() == EXIT_FAILURE)
    {
        return EXIT_FAILURE;
    }

    return 0;
}

void Engine::run()
{
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!window.shouldClose()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                VkExtent2D newExtent = window.getExtent();
                renderer.recreateSwapchain(newExtent);
            }
            window.pollEvents();
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Update the renderer
        renderer.update(deltaTime);

        renderer.drawFrame();
    }
    device.waitIdle();
}


void Engine::cleanup()
{
    swapchain.cleanup();  // Clean up swapchain resources first.

    // Clean up renderer resources 
    renderer.cleanup();

    // Clean up device-specific resources like buffers, memory, etc.
    device.cleanup();

    // Clean up the Vulkan instance.
    instance.cleanup();

    // Clean up window
    window.cleanup();
}

int Engine::initWindow()
{
    try {
        window.create(WIDTH, HEIGHT, "VulkanoVista");
        windowExtent = { WIDTH, HEIGHT };
    }
    catch (std::runtime_error &e) {
        Logger::error("Failed to create window error: " + std::string(e.what()));
        return EXIT_FAILURE;
    }

    return 0;
}

int Engine::initVulkan()
{
    try {
        instance.create(window.getSDLWindow());
        window.createSurface(instance);
        instance.SetSurface(window.getSurface());

        device.pickPhysicalDevice(instance, window.getSurface());
        device.createLogicalDevice(window.getSurface());
        swapchain.create(&device, window.getSurface(), windowExtent);
        renderer.setup(&device, &swapchain, &window);

        std::vector<Vertex> vertices = {
           {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // Bottom vertex (red)
           {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},   // Right vertex (green)
           {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}   // Left vertex (blue)
        };

        std::vector<uint32_t> indices = { 0, 1, 2 };

        // Create the mesh
        auto mesh = std::make_shared<Mesh>(&device, vertices, indices);

        // Add the mesh to the renderer
        renderer.addMesh(mesh);

        std::vector<Vertex> vertices2 = {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},     // Bottom vertex (red)
            {{-0.25f, 0.25f, 0.0f}, {0.0f, 1.0f, 0.0f}},    // Right vertex (green)
            {{-0.75f, 0.25f, 0.0f}, {0.0f, 0.0f, 1.0f}}     // Left vertex (blue)
        };

        std::vector<uint32_t> indices2 = { 0, 1, 2 };

        // Create the mesh
        auto mesh2 = std::make_shared<Mesh>(&device, vertices2, indices2);

        // Add the mesh to the renderer
        renderer.addMesh(mesh2);

        renderer.finalizeSetup();
    }
    catch (std::runtime_error& e) {
        Logger::error("Failed to init Vulkan: " + std::string(e.what()));
        return EXIT_FAILURE;
    }

    return 0;
}
