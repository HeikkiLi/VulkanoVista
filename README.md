# VulkanoVista
Vulkan based rendering project that uses **SDL2** for window management, **GLM** for math operations, and **Assimp** for model loading.
Some of the key Vulkan features implemented:
* swapchain
* render pipeline
* descriptor sets and uniform buffers
* push constants
* depth buffer
* second sub pass
* textures and mesh rendering

## Prerequisites

### Vulkan SDK
Install the Vulkan SDK from [LunarG](https://vulkan.lunarg.com/sdk/home).
- **Windows:** Install Vulkan SDK and set up environment variables using the provided setup script. In the installation in select components choose "GLM Headers" and "SDL2 libraries and headers" or install them separately.


### SDL2
Windows:

If not installed together with the Vulkan SDK then:
Install SDL2 development libraries following these [instructions](https://wiki.libsdl.org/SDL2/Installation).


### GLM
  If OpenGL Mathematics (GLM) is not installed from Vulkan SDK then it can be retrieved from [glm github](https://github.com/g-truc/glm) and configured manually to the VS project.

### Assimp

  Windows: Download Assimp binaries and install from [here](https://kimkulling.itch.io/the-asset-importer-lib) or clone and build from Assim [github repository](https://github.com/assimp/assimp). Add assimp folder to path as "ASSIMP". build assimp following instuctions in Assimp readme files. 
  * Copy assimp dll and lib to folders external\lib\Debug and  external\lib\Release from the assimp build Debug and Release.
  * Copy assimp header files to external\include\assimp

## Building
Build project in Visual Studio. Build shaders running script shaders\compile_shaders.bat

### Folder Structure

```
/VulkanRenderer
├── src/            # Source files
├── external/       # External libraries (assimp include and lib etc.)
├── assets/         # Models & textures
├── shaders/        # Shaders and shader build scripts
└── README.md       # This file
```
