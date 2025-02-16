cmake_minimum_required(VERSION 3.19.2)

project(engine VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_STATIC_VULKAN ON CACHE BOOL "" FORCE)
set(ALLOW_EXTERNAL_SPIRV_TOOLS ON)

option(DEBUG ON)

add_subdirectory(libraries/vulkan/Vulkan-Headers-main)
add_subdirectory(libraries/vulkan/Vulkan-Loader-main)
add_subdirectory(libraries/glfw/glfw-3.3.9)
add_subdirectory(libraries/glslang)
add_subdirectory(libraries/freetype)
add_subdirectory(libraries/fmt)

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

target_link_libraries(engine vulkan glfw glslang freetype fmt::fmt)

if (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
	message("building for win64")
	set(PHYSX_DIR ${PROJECT_SOURCE_DIR}/libraries/physx/win64/checked)
	target_link_libraries(engine
		${PHYSX_DIR}/libs/PhysXExtensions_static_64.lib
		${PHYSX_DIR}/libs/PhysX_64.lib
		${PHYSX_DIR}/libs/PhysXPvdSDK_static_64.lib
		${PHYSX_DIR}/libs/PhysXCommon_64.lib
		${PHYSX_DIR}/libs/PhysXFoundation_64.lib
	)
endif()

if (DEBUG)
	add_compile_definitions(_DEBUG)
else ()
	add_compile_definitions(NDEBUG)
endif ()

target_include_directories(engine PUBLIC
	headers 
	headers/imgui
	libraries/vulkan/Vulkan-Headers-main/include
	libraries/glfw/glfw-3.3.9/include
	libraries/glslang/glslang/include
	libraries/freetype/include
	libraries/fmt/include
	libraries/physx/include
)
