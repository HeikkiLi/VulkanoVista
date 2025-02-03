# VulkanoVista
Vulkan based rendering project that uses **SDL2** for window management, **GLM** for math operations, and **Assimp** for model loading.

## Prerequisites

### Vulkan SDK
Install the Vulkan SDK from [LunarG](https://vulkan.lunarg.com/sdk/home).
- **Windows:** Install Vulkan SDK and set up environment variables using the provided setup script.
- **Linux (Ubuntu-based):**
  ```
  sudo apt update
  sudo apt install vulkan-sdk
  ```

### SDL2

Windows: Download SDL2 development libraries from here and extract them.

Linux (Ubuntu-based):

    ```
    sudo apt install libsdl2-dev
    ```

### GLM (Bundled in the Project)

  No additional setup is required. GLM is included in the externals/ folder.

### Assimp

  Windows: Download Assimp binaries from here.
   Linux:

    sudo apt install libassimp-dev

### Building the Project

Clone the repository

```
git clone https://github.com/yourusername/vulkan-renderer.git
cd vulkan-renderer
```

Create a Build Directory
```
mkdir build && cd build
```

Run CMake

Windows
```
cmake .. -G "Visual Studio 17 2022"
```
Linux
```
cmake ..
```

Compile the Project

  Windows (Visual Studio) Open the generated .sln file and build the project.
  
  Linux

    make

Run the Application

    ./VulkanRenderer

### Folder Structure

```
/VulkanRenderer
├── src/            # Source files
├── externals/      # External libraries (GLM, SDL2, etc.)
├── assets/         # Models & textures
├── CMakeLists.txt  # CMake build file
└── README.md       # This file
```
