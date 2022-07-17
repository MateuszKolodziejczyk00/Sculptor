#include "GLFWWindow.h"

#if USE_GLFW

#include <GLFW/glfw3.h>


namespace spt::lib
{

struct WINDOW_API GLFWWindowData
{
	GLFWWindowData()
		: m_WindowHandle(nullptr)
	{ }

	GLFWwindow* m_WindowHandle;
};


GLFWWindow::GLFWWindow(std::string_view name, math::Vector2i resolution)
{
	m_WindowData = std::make_unique<GLFWWindowData>();

	InitializeWindow(name, resolution);
}

GLFWWindow::~GLFWWindow()
{
	glfwDestroyWindow(m_WindowData->m_WindowHandle);
	glfwTerminate();
}

void GLFWWindow::Update(float deltaTime)
{
	glfwPollEvents();
}

bool GLFWWindow::ShouldClose()
{
	return static_cast<bool>(glfwWindowShouldClose(m_WindowData->m_WindowHandle));
}

void GLFWWindow::InitializeWindow(std::string_view name, math::Vector2i resolution)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_WindowData->m_WindowHandle = glfwCreateWindow(resolution.x(), resolution.y(), name.data(), NULL, NULL);
}

}

#endif // USE_GLFW
