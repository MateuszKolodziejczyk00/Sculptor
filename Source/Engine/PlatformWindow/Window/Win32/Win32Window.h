#pragma once

#include "PlatformWindowMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"
#include "UIContext.h"


#if USE_WIN32


namespace spt::platf
{

struct Win32WindowData;


class PLATFORM_WINDOW_API Win32Window
{
public:

	static void							Initialize();

	Win32Window(lib::StringView name, math::Vector2u resolution);
	~Win32Window();

	math::Vector2u						GetFramebufferSize() const;

	Bool								IsFocused() const;

	void								UpdateRHISurface() const;
	IntPtr								GetRHISurfaceHandle() const;

	void								BeginFrame();
	void								Update();

	Bool								ShouldClose();

	// Removes system caption, keeps resize borders. Title bar must be drawn by the application
	void								EnableCustomTitleBar();

	// Starts system move loop, must be called while left mouse button is pressed
	void								BeginDragMove();
	void								Minimize();
	void								ToggleMaximize();
	Bool								IsWindowMaximized() const;
	void								Close();

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

	void								InitializeWindow(lib::StringView name, math::Vector2u resolution);

	lib::UniquePtr<Win32WindowData>		m_windowData;

	ui::UIContext						m_uiContext;
};

} // spt::platf

#endif // USE_WIN32
