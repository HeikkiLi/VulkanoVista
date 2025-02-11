# VulkanoVista
A simple Vulkan-based rendering project for learning and experimentation.

This project uses SDL2 for window management, GLM for math operations, and Assimp for model loading. It implements basic rendering features such as:
* Swapchain & render pipeline
* Descriptor sets & uniform buffers
* Depth buffering & multi-pass rendering
* Basic texture and mesh rendering
  
<p align="center">
  <img src="https://github.com/user-attachments/assets/0182032e-b0cb-4e1d-a2b6-03411b94bf9b" width="45%">
  <img src="https://github.com/user-attachments/assets/c4fd0e5c-f65b-4e91-bc13-15c00ff7f490" width="45%">
</p>


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
