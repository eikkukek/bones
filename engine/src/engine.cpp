#include "renderer.hpp"
#include <iostream>

namespace engine {

	void Renderer::PrintMsg(const char* msg) {
		std::cout << "renderer message: " << msg << '\n';
	}

	void Renderer::PrintWarn(const char* warn) {
		std::cout << "renderer warning: " << warn << '\n';
	}

	void Renderer::PrintErr(const char* err, Renderer::ErrorOrigin origin) {
		std::cout << "renderer error: " << err << " error origin: " << Renderer::ErrorOriginStr(origin) << '\n';
	}
}
