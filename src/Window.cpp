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
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        Logger::error("Failed to initialize SDL: " + std::string(SDL_GetError()));
        throw std::runtime_error("Failed to initialize SDL");
    }

    // Create SDL window
    sdlWindow = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_VULKAN);
    if (!sdlWindow) {
        Logger::error("Failed to create SDL window: " + std::string(SDL_GetError()));
        throw std::runtime_error("Failed to create SDL window");
    }

    Logger::info("Window created successfully.");
}

void Window::createSurface(const Instance& instance) 
{
    if (!SDL_Vulkan_CreateSurface(sdlWindow, instance.getInstance(), &surface)) {
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
        if (event.type == SDL_QUIT) {
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
