# VulkanoVista
A simple Vulkan-based rendering project for learning and experimentation.

This project uses SDL3 for window management, Dear ImGui for UI, GLM for math operations, and Assimp for model loading. It implements basic rendering features such as:
* Swapchain & render pipeline
* Descriptor sets & uniform buffers
* Depth buffering & multi-pass rendering
* Basic texture and mesh rendering
  
<p align="center">
  <img src="https://github.com/user-attachments/assets/32b163c2-0bfe-49ce-b954-42868394b62f" width="45%">
  <img src="https://github.com/user-attachments/assets/d6a011b0-ba63-4037-8d41-8be73df4d92c" width="45%">
</p>



## **Dependencies**

### **1. Vulkan SDK**
- **Download & Install:** [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home)

### **2. CMake**
- **Windows:** Install [CMake](https://cmake.org/download/) and add it to `PATH`.
- **Ubuntu:** Install via:
```
  sudo apt update
  sudo apt install cmake
```

### **3. vcpkg and assimp**
Clone & Install vcpkg:
```sh
    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.bat   # Windows
    ./bootstrap-vcpkg.sh    # Linux/macOS
```

Install Dependencies:
```sh
    vcpkg install sdl3[x64-windows] glm[x64-windows] imgui[x64-windows] assimp[x64-windows]
```

### **4. Required Packages (Linux)**
Install:
```sh
    sudo apt install libvulkan-dev libsdl2-dev libglm-dev libassimp-dev libglfw3-dev
```


# Building the Project

## Windows (Using vcpkg)
```sh
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

## Linux
```sh
cmake -B build -S .
cmake --build build --config Release
```

# Running the Application
After building, the executable will be located in:

- **Windows:** `build/Release/VulkanoVista.exe`
- **Linux:** `build/VulkanoVista`

# Cloning the Repository
This project uses Git submodules for external dependencies. To properly clone the repository, use:
```sh
git clone --recursive <repository-url>
```
initialize and update submodules:
```sh
git submodule update --init --recursive
```

### Folder Structure

```
/VulkanoVista
├── src/            # Source files
├── external/       # External libraries (assimp include and lib etc.)
├── assets/         # Models & textures
├── shaders/        # Shaders and shader build scripts
└── README.md       # This file
```
