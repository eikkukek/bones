cmake_minimum_required(VERSION 3.19.2)

project(engine VERSION 1.0.0)

option(ENGINE_RELEASE_MODE OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_STATIC_VULKAN ON CACHE BOOL "" FORCE)

set(ALLOW_EXTERNAL_SPIRV_TOOLS ON CACHE BOOL "" FORCE)

set(JPH_SHADERD_LIBRARY OFF CACHE BOOL "" FORCE)
set(JPH_DEBUG_RENDERER OFF CACHE BOOL "" FORCE)
set(JPH_USE_STD_VECTOR OFF CACHE BOOL "" FORCE)

if (ENGINE_RELEASE_MODE)
	message(STATUS "Building engine in release mode")
	set(JPH_ENABLE_ASSERTS OFF)
else()
	message(STATUS "Building engine in debug mode")
	set(JPH_ENABLE_ASSERTS ON)
endif()

add_subdirectory(libraries/vulkan/Vulkan-Headers-main)
add_subdirectory(libraries/vulkan/Vulkan-Loader-main)
add_subdirectory(libraries/glfw/glfw-3.3.9)
add_subdirectory(libraries/glslang)
add_subdirectory(libraries/freetype)
add_subdirectory(libraries/fmt)
add_subdirectory(libraries/jolt/Build)

add_library(engine STATIC
	src/renderer.cpp
	src/text_renderer.cpp
	src/engine.cpp
	src/builtin_pipelines.cpp

	src/imgui/imgui.cpp
	src/imgui/imgui_draw.cpp
	src/imgui/imgui_impl_glfw.cpp
	src/imgui/imgui_impl_vulkan.cpp
	src/imgui/imgui_tables.cpp
	src/imgui/imgui_widgets.cpp
)

target_link_libraries(engine vulkan glfw glslang freetype fmt::fmt Jolt)

target_include_directories(engine PUBLIC
	headers 
	headers/imgui
	libraries/vulkan/Vulkan-Headers-main/include
	libraries/glfw/glfw-3.3.9/include
	libraries/glslang/glslang/include
	libraries/freetype/include
	libraries/fmt/include
)
