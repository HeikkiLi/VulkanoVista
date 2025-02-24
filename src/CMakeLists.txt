file(GLOB_RECURSE SOURCES "*.cpp" "*.h")

add_executable(VulkanoVista ${SOURCES})

set_target_properties(VulkanoVista PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
)

# Ensure only VulkanoVista appears in Visual Studio solution
set_property(TARGET VulkanoVista PROPERTY FOLDER "")

target_include_directories(VulkanoVista PRIVATE
    ${CMAKE_SOURCE_DIR}/external/glm
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_SOURCE_DIR}/external/assimp/include
    ${CMAKE_SOURCE_DIR}/external/zep/include
)

target_link_libraries(VulkanoVista PRIVATE external)
