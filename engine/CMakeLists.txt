cmake_minimum_required(VERSION 3.19.2)

project(engine VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_STATIC_VULKAN ON CACHE BOOL "" FORCE)
set(ALLOW_EXTERNAL_SPIRV_TOOLS ON)

add_subdirectory(libraries/vulkan/Vulkan-Headers-main)
add_subdirectory(libraries/vulkan/Vulkan-Loader-main)
add_subdirectory(libraries/glfw/glfw-3.3.9)
add_subdirectory(libraries/glslang)

add_library(engine STATIC
	src/engine.cpp
)

target_link_libraries(engine vulkan glfw glslang)

target_include_directories(engine
	PUBLIC headers 
	PUBLIC headers/imgui
	PUBLIC libraries/vulkan/Vulkan-Headers-main/include
	PUBLIC libraries/glfw/glfw-3.3.9/include
	PUBLIC libraries/glslang/glslang/include
)
