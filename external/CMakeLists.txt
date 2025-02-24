# External Dependencies

find_package(Vulkan REQUIRED)

find_package(assimp CONFIG REQUIRED)

add_subdirectory(sdl)
add_subdirectory(glm)
add_subdirectory(imgui)

# Add Zep
# If you want to use a single header instead of a library, uncomment this
# set(ZEP_LIB 1)
if (DEFINED ZEP_LIB)
add_subdirectory(zep/src)
else()
add_definitions(-DZEP_SINGLE_HEADER=1)
endif()


# Create an INTERFACE library for linking
add_library(external INTERFACE)

# Include directories for external
target_include_directories(external INTERFACE
    zep/include
    ${ASSIMP_INCLUDE_DIRS}
)

target_link_libraries(external INTERFACE
    SDL3::SDL3
    Vulkan::Vulkan
    glm::glm
    assimp::assimp
    imgui
)

if (DEFINED ZEP_LIB)
target_link_libraries(VulkanoVista PRIVATE Zep::Zep)
endif()
