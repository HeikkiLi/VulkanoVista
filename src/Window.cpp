#include "Window.h"

#include "Logger.h"


Window::~Window()
{
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
}

void Window::create(int width, int height, const std::string& title) 
{
    Logger::info("Creating window with title: " + title);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
    {
        Logger::error("Failed to initialize SDL: " + std::string(SDL_GetError()));
        throw std::runtime_error("Failed to initialize SDL");
    }

    if (!SDL_Vulkan_LoadLibrary(nullptr)) 
    {
        Logger::error("Failed to load Vulkan library: " + std::string(SDL_GetError()));
        throw std::runtime_error("Failed to load Vulkan library");
    }

    // Create SDL window
    sdlWindow = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!sdlWindow)
    {
        Logger::error("Failed to create SDL window: " + std::string(SDL_GetError()));
        throw std::runtime_error("Failed to create SDL window");
    }

    Logger::info("Window created successfully.");
}

void Window::createSurface(const Instance& instance) 
{
    if (!SDL_Vulkan_CreateSurface(sdlWindow, instance.getInstance(), nullptr, &surface)) 
    {
        throw std::runtime_error("Failed to create Vulkan surface!");
    }
}

bool Window::shouldClose() const
{
    return isClosed;
}

void Window::pollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT)
        {
            isClosed = true;  // Set the flag when the window is closed
        }
    }
}

VkSurfaceKHR Window::getSurface() const
{
    return surface;
}

VkExtent2D Window::getExtent() const
{
    int width, height;
    SDL_GetWindowSize(sdlWindow, &width, &height);
    return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
}

void Window::cleanup() 
{
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
}
