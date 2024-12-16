#include "engine.hpp"

using namespace engine;

int main() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* pWindow = glfwCreateWindow(540, 540, "Test", nullptr, nullptr);
	Engine engine("Test", pWindow, 0, nullptr, 1000);
	while (!glfwWindowShouldClose(pWindow)) {
		glfwPollEvents();
		engine.DrawLoop();
	}
	glfwTerminate();
}
