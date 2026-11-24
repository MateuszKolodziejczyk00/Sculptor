#include "Win32Window.h"

#if USE_WIN32

#include "Logging/Log.h"
#include "RHIBridge/RHIImpl.h"
#include "RHICore/RHIInitialization.h"
#include "RHICore/RHIPlugin.h"
#include "Win32InputAdapter.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <windowsx.h>

#if SPT_VULKAN_RHI
#include <vulkan/vulkan_win32.h>
#endif // SPT_VULKAN_RHI

#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


namespace spt::platf
{

SPT_DEFINE_LOG_CATEGORY(Win32Window, true)


struct Win32WindowData
{
	HWND windowHandle = nullptr;

	Win32Window::OnWindowResizedDelegate onResized;
	Win32Window::OnWindowClosedDelegate onClosed;
	Win32Window::OnRHISurfaceUpdate onRHISurfaceUpdate;

	IntPtr rhiSurfaceHandle = 0;

	Uint32 width  = 0u;
	Uint32 height = 0u;

	Bool shouldClose    = false;
	Bool hasUI          = false;
	Bool customTitleBar = false;

	Win32InputAdapter inputAdapter;
};

namespace priv
{

static constexpr const wchar_t* windowClassName = L"SculptorWindowClass";

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

#if SPT_VULKAN_RHI

static void InitializeRHISurface(HWND windowHandle, Win32WindowData& windowData)
{
	const PFN_vkCreateWin32SurfaceKHR createSurface = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(vkGetInstanceProcAddr(rhi::RHI::GetInstanceHandle(), "vkCreateWin32SurfaceKHR"));
	SPT_CHECK(!!createSurface);

	VkWin32SurfaceCreateInfoKHR surfaceInfo{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	surfaceInfo.hinstance = GetModuleHandleW(nullptr);
	surfaceInfo.hwnd      = windowHandle;

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	createSurface(rhi::RHI::GetInstanceHandle(), &surfaceInfo, rhi::RHI::GetAllocationCallbacks(), &surface);
	SPT_CHECK(!!surface);

	const IntPtr newSurfaceHandle = reinterpret_cast<IntPtr>(surface);

	windowData.onRHISurfaceUpdate.Broadcast(windowData.rhiSurfaceHandle, newSurfaceHandle);

	windowData.rhiSurfaceHandle = newSurfaceHandle;
}

#else

#error Only Vulkan is supported

#endif // SPT_VULKAN_RHI

static std::wstring ToWideString(lib::StringView str)
{
	if (str.empty())
	{
		return std::wstring();
	}

	const int length = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
	std::wstring result(static_cast<SizeType>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), length);
	return result;
}

static LONG GetFrameThickness(HWND windowHandle)
{
	const UINT dpi = GetDpiForWindow(windowHandle);
	return GetSystemMetricsForDpi(SM_CYFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Window procedure ==============================================================================

static void OnKeyMessage(Win32WindowData& windowData, WPARAM wParam, LPARAM lParam, Bool isDown)
{
	const Uint32 scanCode  = static_cast<Uint32>((lParam >> 16) & 0xFF);
	const Bool isExtended  = ((lParam >> 24) & 1) != 0;
	const Bool wasDown     = ((lParam >> 30) & 1) != 0;

	// PrintScreen sends only key up message
	if (wParam == VK_SNAPSHOT && !isDown)
	{
		windowData.inputAdapter.OnKeyboardMessage(static_cast<Uint32>(wParam), scanCode, isExtended, true, false);
	}

	windowData.inputAdapter.OnKeyboardMessage(static_cast<Uint32>(wParam), scanCode, isExtended, isDown, isDown && wasDown);
}

static void OnMouseButtonMessage(HWND windowHandle, Win32WindowData& windowData, WPARAM wParam, Uint32 virtualKey, Bool isDown)
{
	windowData.inputAdapter.OnMouseButtonMessage(virtualKey, isDown);

	// Keep receiving mouse messages when dragging outside of the window
	if (isDown)
	{
		SetCapture(windowHandle);
	}
	else if ((wParam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2)) == 0 && GetCapture() == windowHandle)
	{
		ReleaseCapture();
	}
}

static LRESULT CALLBACK WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_NCCREATE)
	{
		const CREATESTRUCTW* createInfo = reinterpret_cast<const CREATESTRUCTW*>(lParam);
		SetWindowLongPtrW(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createInfo->lpCreateParams));
		return DefWindowProcW(windowHandle, message, wParam, lParam);
	}

	Win32WindowData* windowData = reinterpret_cast<Win32WindowData*>(GetWindowLongPtrW(windowHandle, GWLP_USERDATA));
	if (!windowData)
	{
		return DefWindowProcW(windowHandle, message, wParam, lParam);
	}

	if (windowData->hasUI)
	{
		if (const LRESULT result = ImGui_ImplWin32_WndProcHandler(windowHandle, message, wParam, lParam))
		{
			return result;
		}
	}

	switch (message)
	{
	case WM_SIZE:
	{
		const Uint32 newWidth  = static_cast<Uint32>(LOWORD(lParam));
		const Uint32 newHeight = static_cast<Uint32>(HIWORD(lParam));

		if (newWidth != windowData->width || newHeight != windowData->height)
		{
			SPT_PROFILER_FUNCTION();

			windowData->width  = newWidth;
			windowData->height = newHeight;

			windowData->onResized.Broadcast(newWidth, newHeight);
		}
		return 0;
	}

	case WM_CLOSE:
		windowData->shouldClose = true;
		windowData->onClosed.Broadcast();
		return 0;

	case WM_ERASEBKGND:
		return 1;

	case WM_NCCALCSIZE:
		if (windowData->customTitleBar && wParam == TRUE)
		{
			// Keep system frame on left, right and bottom, drop caption. Maximized window extends beyond monitor by frame thickness
			NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
			const LONG top = params->rgrc[0].top;
			DefWindowProcW(windowHandle, message, wParam, lParam);
			params->rgrc[0].top = top + (IsZoomed(windowHandle) ? GetFrameThickness(windowHandle) : 0);
			return 0;
		}
		break;

	case WM_NCHITTEST:
		if (windowData->customTitleBar)
		{
			const LRESULT hit = DefWindowProcW(windowHandle, message, wParam, lParam);

			POINT cursor{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			ScreenToClient(windowHandle, &cursor);

			return (hit == HTCLIENT && !IsZoomed(windowHandle) && cursor.y < GetFrameThickness(windowHandle)) ? HTTOP : hit;
		}
		break;

	case WM_KEYDOWN:
		OnKeyMessage(*windowData, wParam, lParam, true);
		return 0;

	case WM_KEYUP:
		OnKeyMessage(*windowData, wParam, lParam, false);
		return 0;

	// System key messages must reach DefWindowProc (Alt+F4, etc.)
	case WM_SYSKEYDOWN:
		OnKeyMessage(*windowData, wParam, lParam, true);
		break;

	case WM_SYSKEYUP:
		OnKeyMessage(*windowData, wParam, lParam, false);
		break;

	case WM_LBUTTONDOWN:
		OnMouseButtonMessage(windowHandle, *windowData, wParam, VK_LBUTTON, true);
		return 0;

	case WM_LBUTTONUP:
		OnMouseButtonMessage(windowHandle, *windowData, wParam, VK_LBUTTON, false);
		return 0;

	case WM_RBUTTONDOWN:
		OnMouseButtonMessage(windowHandle, *windowData, wParam, VK_RBUTTON, true);
		return 0;

	case WM_RBUTTONUP:
		OnMouseButtonMessage(windowHandle, *windowData, wParam, VK_RBUTTON, false);
		return 0;

	case WM_MBUTTONDOWN:
		OnMouseButtonMessage(windowHandle, *windowData, wParam, VK_MBUTTON, true);
		return 0;

	case WM_MBUTTONUP:
		OnMouseButtonMessage(windowHandle, *windowData, wParam, VK_MBUTTON, false);
		return 0;

	case WM_XBUTTONDOWN:
		OnMouseButtonMessage(windowHandle, *windowData, wParam, GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2, true);
		return TRUE;

	case WM_XBUTTONUP:
		OnMouseButtonMessage(windowHandle, *windowData, wParam, GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2, false);
		return TRUE;

	case WM_MOUSEMOVE:
		windowData->inputAdapter.OnMousePositionChanged(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_MOUSEWHEEL:
		windowData->inputAdapter.OnScrollPositionChanged(static_cast<Real32>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<Real32>(WHEEL_DELTA));
		return 0;
	}

	return DefWindowProcW(windowHandle, message, wParam, lParam);
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIPlugin =====================================================================================

class Win32RHIPlugin : public rhi::RHIPlugin
{
protected:

	using Super = rhi::RHIPlugin;

public:

	virtual void CollectExtensions(rhi::ExtensionsCollector& collector) const override;
};


void Win32RHIPlugin::CollectExtensions(rhi::ExtensionsCollector& collector) const
{
	Super::CollectExtensions(collector);

	collector.AddRHIExtension(lib::StringView(VK_KHR_SURFACE_EXTENSION_NAME));
	collector.AddRHIExtension(lib::StringView(VK_KHR_WIN32_SURFACE_EXTENSION_NAME));
}

Win32RHIPlugin g_win32RHIPlugin;

//////////////////////////////////////////////////////////////////////////////////////////////////
// Win32Window ===================================================================================

void Win32Window::Initialize()
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	WNDCLASSEXW windowClass{};
	windowClass.cbSize        = sizeof(windowClass);
	windowClass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	windowClass.lpfnWndProc   = &priv::WindowProc;
	windowClass.hInstance     = GetModuleHandleW(nullptr);
	windowClass.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
	windowClass.lpszClassName = priv::windowClassName;

	if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
	{
		SPT_LOG_ERROR(Win32Window, "Failed to register window class");
	}
}

Win32Window::Win32Window(lib::StringView name, math::Vector2u resolution)
	: m_windowData(std::make_unique<Win32WindowData>())
{
	InitializeWindow(name, resolution);
}

Win32Window::~Win32Window()
{
	DestroyWindow(m_windowData->windowHandle);
}

math::Vector2u Win32Window::GetFramebufferSize() const
{
	RECT clientRect{};
	GetClientRect(m_windowData->windowHandle, &clientRect);
	return math::Vector2u(static_cast<Uint32>(clientRect.right - clientRect.left), static_cast<Uint32>(clientRect.bottom - clientRect.top));
}

Bool Win32Window::IsFocused() const
{
	return GetActiveWindow() == m_windowData->windowHandle;
}

void Win32Window::UpdateRHISurface() const
{
	priv::InitializeRHISurface(m_windowData->windowHandle, *m_windowData);
}

IntPtr Win32Window::GetRHISurfaceHandle() const
{
	return m_windowData->rhiSurfaceHandle;
}

void Win32Window::BeginFrame()
{
	SPT_PROFILER_FUNCTION();

	if (HasValidUIContext())
	{
		ImGui_ImplWin32_NewFrame();
	}
}

void Win32Window::Update()
{
	SPT_PROFILER_FUNCTION();

	m_windowData->inputAdapter.PreUpdateInput();

	MSG message{};
	while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
}

Bool Win32Window::ShouldClose()
{
	return m_windowData->shouldClose;
}

void Win32Window::EnableCustomTitleBar()
{
	m_windowData->customTitleBar = true;

	SetWindowPos(m_windowData->windowHandle, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Win32Window::BeginDragMove()
{
	SPT_CHECK(m_windowData->customTitleBar);

	ReleaseCapture();
	SendMessageW(m_windowData->windowHandle, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);

	// Move loop consumes button release
	SendMessageW(m_windowData->windowHandle, WM_LBUTTONUP, 0, 0);
}

void Win32Window::Minimize()
{
	ShowWindow(m_windowData->windowHandle, SW_MINIMIZE);
}

void Win32Window::ToggleMaximize()
{
	ShowWindow(m_windowData->windowHandle, IsWindowMaximized() ? SW_RESTORE : SW_MAXIMIZE);
}

Bool Win32Window::IsWindowMaximized() const
{
	return IsZoomed(m_windowData->windowHandle) != FALSE;
}

void Win32Window::Close()
{
	PostMessageW(m_windowData->windowHandle, WM_CLOSE, 0, 0);
}

Win32Window::OnWindowResizedDelegate& Win32Window::GetOnResizedCallback()
{
	return m_windowData->onResized;
}

Win32Window::OnWindowClosedDelegate& Win32Window::GetOnClosedCallback()
{
	return m_windowData->onClosed;
}

Win32Window::OnRHISurfaceUpdate& Win32Window::OnRHISurfaceUpdateCallback()
{
	return m_windowData->onRHISurfaceUpdate;
}

void Win32Window::InitializeUI(ui::UIContext context)
{
	m_uiContext = context;

	ImGui::SetCurrentContext(context.GetHandle());

	ImGui_ImplWin32_Init(m_windowData->windowHandle);

	m_windowData->hasUI = true;
}

void Win32Window::UninitializeUI()
{
	SPT_CHECK(HasValidUIContext());

	m_windowData->hasUI = false;

	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext(GetUIContext().GetHandle());
	m_uiContext.Reset();
}

Bool Win32Window::HasValidUIContext() const
{
	return GetUIContext().IsValid();
}

ui::UIContext Win32Window::GetUIContext() const
{
	return m_uiContext;
}

void Win32Window::InitializeWindow(lib::StringView name, math::Vector2u resolution)
{
	const DWORD style   = WS_OVERLAPPEDWINDOW;
	const DWORD exStyle = WS_EX_APPWINDOW;

	// Requested resolution describes client area
	RECT windowRect{ 0, 0, static_cast<LONG>(resolution.x()), static_cast<LONG>(resolution.y()) };
	AdjustWindowRectExForDpi(&windowRect, style, FALSE, exStyle, GetDpiForSystem());

	const std::wstring wideName = priv::ToWideString(name);

	HWND windowHandle = CreateWindowExW(exStyle,
										priv::windowClassName,
										wideName.c_str(),
										style,
										CW_USEDEFAULT, CW_USEDEFAULT,
										windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
										nullptr, nullptr,
										GetModuleHandleW(nullptr),
										m_windowData.get());

	SPT_CHECK(!!windowHandle);

	m_windowData->windowHandle = windowHandle;

	const math::Vector2u framebufferSize = GetFramebufferSize();
	m_windowData->width  = framebufferSize.x();
	m_windowData->height = framebufferSize.y();

	ShowWindow(windowHandle, SW_SHOWNORMAL);

	priv::InitializeRHISurface(windowHandle, *m_windowData);
}

} // spt::platf

#endif // USE_WIN32
