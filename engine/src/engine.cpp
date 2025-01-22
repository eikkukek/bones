#include "engine.hpp"

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
}
