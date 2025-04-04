#pragma once

#include "PlatformWindowMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"
#include "UIContext.h"


#if USE_GLFW


namespace spt::platf
{

struct GLFWWindowData;


class PLATFORM_WINDOW_API GLFWWindow
{
public:

	static void							Initialize();

	GLFWWindow(lib::StringView name, math::Vector2u resolution);
	~GLFWWindow();

	math::Vector2u						GetFramebufferSize() const;

	Bool								IsFocused() const;

	void								UpdateRHISurface() const;
	IntPtr								GetRHISurfaceHandle() const;

	void								BeginFrame();
	void								Update();

	Bool								ShouldClose();

	using OnWindowResizedDelegate		= lib::MulticastDelegate<void(Uint32 /*newWidth*/, Uint32 /*newHeight*/)>;
	OnWindowResizedDelegate&			GetOnResizedCallback();

	using OnWindowClosedDelegate		= lib::MulticastDelegate<void()>;
	OnWindowClosedDelegate&				GetOnClosedCallback();

	using OnRHISurfaceUpdate			= lib::MulticastDelegate<void(IntPtr /*prevSurfaceHandle*/, IntPtr /*newSurfaceHandle*/)>;
	OnRHISurfaceUpdate&					OnRHISurfaceUpdateCallback();

	void								InitializeUI(ui::UIContext context);
	void								UninitializeUI();

	Bool								HasValidUIContext() const;
	ui::UIContext						GetUIContext() const;

private:

	void								OnThisWindowClosed();

	void								InitializeWindow(lib::StringView name, math::Vector2u resolution);

	lib::UniquePtr<GLFWWindowData>		m_windowData;

	ui::UIContext						m_uiContext;
};

} // spt::plat

#endif // USE_GLFW
