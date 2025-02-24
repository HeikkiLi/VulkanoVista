#include "Engine.h"

#include <stdexcept>
#include <chrono>

#include "Logger.h"
#include "Mesh.h"
#include <glm/ext/matrix_transform.hpp>

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

    while (!window.shouldClose()) 
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event); // Process ImGui input
            if (event.type == SDL_EVENT_WINDOW_RESIZED) 
            {
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

        // Draw frame
        renderer.drawFrame();

    }
    device.waitIdle();
}


void Engine::cleanup()
{
    swapchain.cleanup();  // Clean up swapchain resources first.
    renderer.cleanup();   // Clean up renderer resources
    device.cleanup();     // Clean up device resources
    instance.cleanup();   // Clean up Vulkan instance
    window.cleanup();     // Clean up window
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
        renderer.setup(&device, &swapchain, &window, &instance);

        //int modelIndex = renderer.createMeshModel("assets/Crate/", "Crate1.obj");
        int modelIndex = renderer.createMeshModel("assets/teapot/", "teapot.obj");

        
        MeshModel meshModel = renderer.getMeshModel(modelIndex);
        glm::vec3 offset(0.0f, -60.0f, -150.0f);
        float rotationAngle = glm::radians(-45.0f);
        glm::vec3 scale(0.1f, 0.1f, 0.1f);
        meshModel.setModel(glm::scale(meshModel.getModel().model, scale));
        meshModel.setModel(glm::translate(meshModel.getModel().model, offset));
        meshModel.setModel(glm::rotate(meshModel.getModel().model, rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f)));
        

        renderer.getMeshModel(modelIndex).setModel(meshModel.getModel().model);
        

        renderer.finalizeSetup();
    }
    catch (std::runtime_error& e) {
        Logger::error("Failed to init Vulkan: " + std::string(e.what()));
        return EXIT_FAILURE;
    }

    return 0;
}
