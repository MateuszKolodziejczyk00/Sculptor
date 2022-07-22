#pragma once

#include "WindowMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"


#if USE_GLFW

namespace spt::window
{

struct GLFWWindowData;


class WINDOW_API GLFWWindow
{
public:

	GLFWWindow(std::string_view name, math::Vector2i resolution);
	~GLFWWindow();

	void Update(Real32 deltaTime);

	Bool ShouldClose();

	using OnWindowResizedDelegate = lib::MulticastDelegate<Uint32 /*newWidth*/, Uint32 /*newHeight*/>;
	OnWindowResizedDelegate& GetOnResizedCallback();

	using OnWindowClosedDelegate = lib::MulticastDelegate<>;
	OnWindowClosedDelegate& GetOnClosedCallback();

private:

	void OnWindowClosed();

	void InitializeWindow(std::string_view name, math::Vector2i resolution);

	lib::UniquePtr<GLFWWindowData> m_windowData;
};

}

#endif // USE_GLFW
