#include "GLFWWindow.h"

#if USE_GLFW

#include "Logging/Log.h"
#include "RHIImpl.h"
#include "RHIInitialization.h"

#include <GLFW/glfw3.h>


namespace spt::window
{

SPT_IMPLEMENT_LOG_CATEGORY(GLFW, true)


struct GLFWWindowData
{
	GLFWWindowData()
		: m_windowHandle(nullptr)
	{ }

	GLFWwindow* m_windowHandle;

	GLFWWindow::OnWindowResizedDelegate m_onResized;
	GLFWWindow::OnWindowClosedDelegate m_onClosed;
};


namespace priv
{

static void OnErrorCallback(int errorCode, const char* description)
{
	SPT_LOG_ERROR(GLFW, description);
}

static void OnWindowResized(GLFWwindow* window, int newWidth, int newHeight)
{
	SPT_PROFILE_FUNCTION();

	GLFWWindowData* windowData = static_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
	SPT_CHECK(!!windowData);

	windowData->m_onResized.Broadcast(static_cast<Uint32>(newWidth), static_cast<Uint32>(newHeight));
}

static void OnWindowClosed(GLFWwindow* window)
{
	GLFWWindowData* windowData = static_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
	SPT_CHECK(!!windowData);

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

#if VULKAN_RHI

static void GetRequiredExtensions(rhicore::RHIInitializationInfo& initializationInfo)
{
	initializationInfo.m_extensions = glfwGetRequiredInstanceExtensions(&initializationInfo.m_extensionsNum);
}

static void PostInitializeRHIInstance(GLFWwindow* windowHandle)
{
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	glfwCreateWindowSurface(rhi::RHI::GetInstanceHandle(), windowHandle, rhi::RHI::GetAllocationCallbacks(), &surface);
	SPT_CHECK(!!surface);

	rhi::RHI::SetSurfaceHandle(surface);
}

#else

#error Only Vulkan is supported

#endif // VULKAN_RHI

static void InitializeRHI(GLFWwindow* windowHandle)
{
	rhicore::RHIInitializationInfo initializationInfo;
	priv::GetRequiredExtensions(initializationInfo);

	rhi::RHI::Initialize(initializationInfo);
	
	PostInitializeRHIInstance(windowHandle);

	rhi::RHI::SelectAndInitializeGPU();
}

} // priv


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

void GLFWWindow::Update(Real32 deltaTime)
{
	glfwPollEvents();
}

Bool GLFWWindow::ShouldClose()
{
	return static_cast<Bool>(glfwWindowShouldClose(m_windowData->m_windowHandle));
}

GLFWWindow::OnWindowResizedDelegate& GLFWWindow::GetOnResizedCallback()
{
	return m_windowData->m_onResized;
}

GLFWWindow::OnWindowClosedDelegate& GLFWWindow::GetOnClosedCallback()
{
	return m_windowData->m_onClosed;
}

void GLFWWindow::InitializeWindow(std::string_view name, math::Vector2i resolution)
{
	glfwSetErrorCallback(&priv::OnErrorCallback);

	if (!glfwInit())
	{
		SPT_LOG_ERROR(GLFW, "GLFW Initialization failed");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWwindow* windowHandle = glfwCreateWindow(resolution.x(), resolution.y(), name.data(), NULL, NULL);

	priv::InitializeRHI(windowHandle);

	m_windowData->m_windowHandle = windowHandle;

	glfwSetWindowUserPointer(windowHandle, m_windowData.get());

	glfwSetWindowSizeCallback(windowHandle, &priv::OnWindowResized);
	glfwSetWindowCloseCallback(windowHandle, &priv::OnWindowClosed);
	glfwSetKeyCallback(windowHandle, &priv::OnKeyAction);
	glfwSetCursorPosCallback(windowHandle, &priv::OnMouseMoved);
	glfwSetMouseButtonCallback(windowHandle, &priv::OnMouseButtonAction);

	GetOnClosedCallback().AddMember(this, &GLFWWindow::OnWindowClosed);
}

void GLFWWindow::OnWindowClosed()
{
	rhi::RHI::Uninitialize();
}

}

#endif // USE_GLFW
