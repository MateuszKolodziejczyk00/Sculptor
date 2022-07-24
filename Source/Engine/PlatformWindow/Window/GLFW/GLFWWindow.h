#pragma once

#include "PlatformWindowMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"
#include "Window/PlatformWindowCommon.h"


#if USE_GLFW

namespace spt::platform
{

struct GLFWWindowData;


class PLATFORM_WINDOW_API GLFWWindow
{
public:

	static void							Initialize();

	static RequiredExtensionsInfo		GetRequiredRHIExtensionNames();

	GLFWWindow(lib::StringView name, math::Vector2u resolution);
	~GLFWWindow();

	math::Vector2u						GetFramebufferSize() const;

	void								Update(Real32 deltaTime);

	Bool								ShouldClose();

	using OnWindowResizedDelegate		= lib::MulticastDelegate<Uint32 /*newWidth*/, Uint32 /*newHeight*/>;
	OnWindowResizedDelegate&			GetOnResizedCallback();

	using OnWindowClosedDelegate		= lib::MulticastDelegate<>;
	OnWindowClosedDelegate&				GetOnClosedCallback();

private:

	void								InitializeWindow(lib::StringView name, math::Vector2u resolution);

	lib::UniquePtr<GLFWWindowData>		m_windowData;
};

}

#endif // USE_GLFW
