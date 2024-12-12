#include "engine.hpp"

int main() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* pWindow = glfwCreateWindow(540, 540, "Test", nullptr, nullptr);
	engine::Engine engine("Test", pWindow, 0, nullptr);
	while (!glfwWindowShouldClose(pWindow)) {
		glfwPollEvents();
		engine.DrawLoop();
	}
	glfwTerminate();
}
