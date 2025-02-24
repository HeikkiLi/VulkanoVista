set(SDL_TEST OFF)
set(BUILD_SHARED_LIBS OFF)

add_subdirectory(SDL)

add_library(SDL3 INTERFACE)
target_link_libraries(SDL3 INTERFACE SDL3::SDL3)
target_include_directories(SDL3 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/SDL/include)
