#include "engine.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

namespace engine {

	void CriticalError(ErrorOrigin origin, const char *err, VkResult vkErr) {
		fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold, 
			"Engine called a critical error!\nError origin: {}\nError: {}\n", ErrorOriginString(origin), err);
		if (vkErr != VK_SUCCESS) {
			fmt::print(fmt::fg(fmt::color::crimson) | fmt::emphasis::bold,
				"Vulkan error code: {}\n", (int)vkErr);
		}
		Engine::s_engine_instance->~Engine();
		Engine::s_engine_instance = nullptr;
		fmt::print(fmt::emphasis::bold, "Stopping program execution...\n");
		glfwTerminate();
#ifdef DEBUG
		assert(false);
#endif
		exit(EXIT_FAILURE);
	}

	bool LoadImage(const char* fileName, uint32_t components, uint8_t*& outImage, Vec2_T<uint32_t>& outExtent) {
		int x, y, texChannels;
		outImage = stbi_load(fileName, &x, &y, &texChannels, components);
		if (!outImage) {
			PrintError(ErrorOrigin::Stb, 
				"failed to load image (function stbi_load in function LoadImage)!");
			return false;
		}
		outExtent = { (uint32_t)x, (uint32_t)y };
		return true;
	}
}
