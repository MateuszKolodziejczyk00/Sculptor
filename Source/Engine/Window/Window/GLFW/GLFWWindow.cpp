#include "GLFWWindow.h"
#include "Logging/Log.h"

#if USE_GLFW

#include <GLFW/glfw3.h>


namespace spt::window
{

IMPLEMENT_LOG_CATEGORY(GLFW, true)

struct GLFWWindowData
{
	GLFWWindowData()
		: m_WindowHandle(nullptr)
	{ }

	GLFWwindow* m_WindowHandle;
};


GLFWWindow::GLFWWindow(std::string_view name, math::Vector2i resolution)
{
	m_windowData = std::make_unique<GLFWWindowData>();

	InitializeWindow(name, resolution);
}

GLFWWindow::~GLFWWindow()
{
	glfwDestroyWindow(m_windowData->m_WindowHandle);
	glfwTerminate();
}

void GLFWWindow::Update(float deltaTime)
{
	glfwPollEvents();
}

bool GLFWWindow::ShouldClose()
{
	return static_cast<bool>(glfwWindowShouldClose(m_windowData->m_WindowHandle));
}

void GLFWWindow::InitializeWindow(std::string_view name, math::Vector2i resolution)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_windowData->m_WindowHandle = glfwCreateWindow(resolution.x(), resolution.y(), name.data(), NULL, NULL);
}

}

#endif // USE_GLFW
