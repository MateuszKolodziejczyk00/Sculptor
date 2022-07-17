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
		: m_windowHandle(nullptr)
	{ }

	GLFWwindow* m_windowHandle;

	GLFWWindow::OnWindowResized m_onResized;
	GLFWWindow::OnWindowClosed m_onClosed;
};


namespace internal
{

static void OnErrorCallback(int errorCode, const char* description)
{
	LOG_ERROR(GLFW, description);
}

static void OnWindowResized(GLFWwindow* window, int newWidth, int newHeight)
{
	GLFWWindowData* windowData = static_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
	CHECK(!!windowData);

	windowData->m_onResized.Broadcast(static_cast<uint32>(newWidth), static_cast<uint32>(newHeight));
}

static void OnWindowClosed(GLFWwindow* window)
{
	GLFWWindowData* windowData = static_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
	CHECK(!!windowData);

	windowData->m_onClosed.Broadcast();
}

static void OnKeyAction(GLFWwindow* window, int key, int scanCode, int action, int mods)
{

}

static void OnMouseButtonAction(GLFWwindow* window, int key, int action, int mods)
{

}

static void OnMouseMoved(GLFWwindow* window, double newX, double newY)
{

}

} // internal


GLFWWindow::GLFWWindow(std::string_view name, math::Vector2i resolution)
{
	m_windowData = std::make_unique<GLFWWindowData>();

	InitializeWindow(name, resolution);
}

GLFWWindow::~GLFWWindow()
{
	glfwDestroyWindow(m_windowData->m_windowHandle);
	glfwTerminate();
}

void GLFWWindow::Update(float deltaTime)
{
	glfwPollEvents();
}

bool GLFWWindow::ShouldClose()
{
	return static_cast<bool>(glfwWindowShouldClose(m_windowData->m_windowHandle));
}

GLFWWindow::OnWindowResized& GLFWWindow::OnResized()
{
	return m_windowData->m_onResized;
}

GLFWWindow::OnWindowClosed& GLFWWindow::OnClosed()
{
	return m_windowData->m_onClosed;
}

void GLFWWindow::InitializeWindow(std::string_view name, math::Vector2i resolution)
{
	glfwSetErrorCallback(&internal::OnErrorCallback);

	if (!glfwInit())
	{
		LOG_ERROR(GLFW, "GLFW Initialization failed");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWwindow* windowHandle = glfwCreateWindow(resolution.x(), resolution.y(), name.data(), NULL, NULL);

	m_windowData->m_windowHandle = windowHandle;

	glfwSetWindowUserPointer(windowHandle, m_windowData.get());

	glfwSetWindowSizeCallback(windowHandle, &internal::OnWindowResized);
	glfwSetWindowCloseCallback(windowHandle, &internal::OnWindowClosed);
	glfwSetKeyCallback(windowHandle, &internal::OnKeyAction);
	glfwSetCursorPosCallback(windowHandle, &internal::OnMouseMoved);
	glfwSetMouseButtonCallback(windowHandle, &internal::OnMouseButtonAction);
}

}

#endif // USE_GLFW
