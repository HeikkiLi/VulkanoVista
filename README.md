# VulkanoVista
Vulkan based rendering project that uses **SDL2** for window management, **GLM** for math operations, and **Assimp** for model loading.

## Prerequisites

### Vulkan SDK
Install the Vulkan SDK from [LunarG](https://vulkan.lunarg.com/sdk/home).
- **Windows:** Install Vulkan SDK and set up environment variables using the provided setup script.


### SDL2

Windows: install SDL2 development libraries following [instructions](https://wiki.libsdl.org/SDL2/Installation).


### GLM (Bundled in the Project)

  No additional setup is required. GLM is included in the externals/ folder.

### Assimp

  Windows: Download Assimp binaries and install from [here](https://kimkulling.itch.io/the-asset-importer-lib) or clone and build from Assim [github repository](https://github.com/assimp/assimp). Add assimp folder to path as "ASSIMP". build assimp following instuctions in Assimp readme files.

### Folder Structure

```
/VulkanRenderer
├── src/            # Source files
├── externals/      # External libraries (GLM, SDL2, etc.)
├── assets/         # Models & textures
└── README.md       # This file
```
