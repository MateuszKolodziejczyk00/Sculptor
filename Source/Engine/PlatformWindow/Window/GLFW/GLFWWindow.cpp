#include "GLFWWindow.h"

#if USE_GLFW

#include "Logging/Log.h"
#include "RHIBridge/RHIImpl.h"
#include "RHICore/RHIInitialization.h"
#include "GLFWInputAdapter.h"

#include <GLFW/glfw3.h>
#include "backends/imgui_impl_glfw.h"


namespace spt::platf
{

SPT_DEFINE_LOG_CATEGORY(GLFW, true)


struct GLFWWindowData
{
	GLFWWindowData()
		: windowHandle(nullptr)
		, rhiSurfaceHandle(0)
	{ }

	GLFWwindow* windowHandle;

	GLFWWindow::OnWindowResizedDelegate onResized;
	GLFWWindow::OnWindowClosedDelegate onClosed;
	GLFWWindow::OnRHISurfaceUpdate onRHISurfaceUpdate;

	IntPtr rhiSurfaceHandle;

	GLFWInputAdapter inputAdapter;
};

namespace priv
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

#if SPT_VULKAN_RHI

static RequiredExtensionsInfo GetRequiredExtensions()
{
	RequiredExtensionsInfo extensionsInfo;
	extensionsInfo.extensions = glfwGetRequiredInstanceExtensions(&extensionsInfo.extensionsNum);
	return extensionsInfo;
}

static void InitializeRHIWindow(GLFWwindow* windowHandle)
{
	GLFWWindowData* windowData = static_cast<GLFWWindowData*>(glfwGetWindowUserPointer(windowHandle));
	SPT_CHECK(!!windowData);

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	glfwCreateWindowSurface(rhi::RHI::GetInstanceHandle(), windowHandle, rhi::RHI::GetAllocationCallbacks(), &surface);
	SPT_CHECK(!!surface);

	const IntPtr newSurfaceHandle = reinterpret_cast<IntPtr>(surface);

	windowData->onRHISurfaceUpdate.Broadcast(windowData->rhiSurfaceHandle, newSurfaceHandle);

	windowData->rhiSurfaceHandle = newSurfaceHandle;
}

#else

#error Only Vulkan is supported

#endif // SPT_VULKAN_RHI

static void InitializeRHISurface(GLFWwindow* windowHandle)
{
	InitializeRHIWindow(windowHandle);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks =====================================================================================

static void OnErrorCallback(int errorCode, const char* description)
{
	SPT_LOG_ERROR(GLFW, description);
}

static void OnWindowResized(GLFWwindow* window, int newWidth, int newHeight)
{
	SPT_PROFILER_FUNCTION();

	GLFWWindowData* windowData = static_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
	SPT_CHECK(!!windowData);

	windowData->onResized.Broadcast(static_cast<Uint32>(newWidth), static_cast<Uint32>(newHeight));
}

static void OnWindowClosed(GLFWwindow* window)
{
	GLFWWindowData* windowData = static_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
	SPT_CHECK(!!windowData);

	windowData->onClosed.Broadcast();
}

static void OnKeyAction(GLFWwindow* window, int key, int scanCode, int action, int mods)
{
	GLFWWindowData* windowData = static_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
	SPT_CHECK(!!windowData);

	windowData->inputAdapter.OnKeyboardKeyAction(key, action);
}

static void OnMouseButtonAction(GLFWwindow* window, int key, int action, int mods)
{
	GLFWWindowData* windowData = static_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
	SPT_CHECK(!!windowData);

	windowData->inputAdapter.OnMouseKeyAction(key, action);
}

static void OnMouseMoved(GLFWwindow* window, double newX, double newY)
{
	GLFWWindowData* windowData = static_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
	SPT_CHECK(!!windowData);

	windowData->inputAdapter.OnMousePositionChanged(newX, newY);
}

static void OnMouseScroll(GLFWwindow* window, double xOffset, double yOffset)
{
	GLFWWindowData* windowData = static_cast<GLFWWindowData*>(glfwGetWindowUserPointer(window));
	SPT_CHECK(!!windowData);

	windowData->inputAdapter.OnScrollPositionChanged(xOffset, yOffset);
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// GLFWWindow ====================================================================================

void GLFWWindow::Initialize()
{
	glfwSetErrorCallback(&priv::OnErrorCallback);

	if (!glfwInit())
	{
		SPT_LOG_ERROR(GLFW, "GLFW Initialization failed");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

RequiredExtensionsInfo GLFWWindow::GetRequiredRHIExtensionNames()
{
	return priv::GetRequiredExtensions();
}

GLFWWindow::GLFWWindow(lib::StringView name, math::Vector2u resolution)
	: m_windowData(std::make_unique<GLFWWindowData>())
{
	InitializeWindow(name, resolution);
}

GLFWWindow::~GLFWWindow()
{
	glfwDestroyWindow(m_windowData->windowHandle);
	glfwTerminate();
}

math::Vector2u GLFWWindow::GetFramebufferSize() const
{
	int width = idxNone<int>;
	int height = idxNone<int>;
	glfwGetFramebufferSize(m_windowData->windowHandle, &width, &height);
	return math::Vector2u(static_cast<Uint32>(width), static_cast<Uint32>(height));
}

void GLFWWindow::UpdateRHISurface() const
{
	priv::InitializeRHISurface(m_windowData->windowHandle);
}

IntPtr GLFWWindow::GetRHISurfaceHandle() const
{
	return m_windowData->rhiSurfaceHandle;
}

void GLFWWindow::BeginFrame()
{
	SPT_PROFILER_FUNCTION();

	if (HasValidUIContext())
	{
		ImGui_ImplGlfw_NewFrame();
	}
}

void GLFWWindow::Update()
{
	SPT_PROFILER_FUNCTION();

	m_windowData->inputAdapter.PreUpdateInput();

	glfwPollEvents();
}

Bool GLFWWindow::ShouldClose()
{
	return static_cast<Bool>(glfwWindowShouldClose(m_windowData->windowHandle));
}

GLFWWindow::OnWindowResizedDelegate& GLFWWindow::GetOnResizedCallback()
{
	return m_windowData->onResized;
}

GLFWWindow::OnWindowClosedDelegate& GLFWWindow::GetOnClosedCallback()
{
	return m_windowData->onClosed;
}

GLFWWindow::OnRHISurfaceUpdate& GLFWWindow::OnRHISurfaceUpdateCallback()
{
	return m_windowData->onRHISurfaceUpdate;
}

void GLFWWindow::InitializeUI(ui::UIContext context)
{
	m_uiContext = context;

	ImGui::SetCurrentContext(context.GetHandle());

#if SPT_VULKAN_RHI

	ImGui_ImplGlfw_InitForVulkan(m_windowData->windowHandle, true);

#endif // VULKANRHI
}

void GLFWWindow::UninitializeUI()
{
	SPT_CHECK(HasValidUIContext());

	ImGui_ImplGlfw_Shutdown();

	ImGui::DestroyContext(GetUIContext().GetHandle());
	m_uiContext.Reset();
}

Bool GLFWWindow::HasValidUIContext() const
{
	return GetUIContext().IsValid();
}

ui::UIContext GLFWWindow::GetUIContext() const
{
	return m_uiContext;
}

void GLFWWindow::OnThisWindowClosed()
{

}

void GLFWWindow::InitializeWindow(lib::StringView name, math::Vector2u resolution)
{
	GLFWwindow* windowHandle = glfwCreateWindow(resolution.x(), resolution.y(), name.data(), NULL, NULL);

	m_windowData->windowHandle = windowHandle;

	glfwSetWindowUserPointer(windowHandle, m_windowData.get());

	priv::InitializeRHISurface(windowHandle);

	glfwSetWindowSizeCallback(windowHandle, &priv::OnWindowResized);
	glfwSetWindowCloseCallback(windowHandle, &priv::OnWindowClosed);
	glfwSetKeyCallback(windowHandle, &priv::OnKeyAction);
	glfwSetCursorPosCallback(windowHandle, &priv::OnMouseMoved);
	glfwSetMouseButtonCallback(windowHandle, &priv::OnMouseButtonAction);
	glfwSetScrollCallback(windowHandle, &priv::OnMouseScroll);

	GetOnClosedCallback().AddRawMember(this, &GLFWWindow::OnThisWindowClosed);
}

} // spt::plat

#endif // USE_GLFW
