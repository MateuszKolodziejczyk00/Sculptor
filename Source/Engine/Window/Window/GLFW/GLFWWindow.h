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

	void Update(float deltaTime);

	bool ShouldClose();

	using OnWindowResized = lib::MulticastDelegate<uint32 /*newWidth*/, uint32 /*newHeight*/>;
	OnWindowResized& OnResized();

	using OnWindowClosed = lib::MulticastDelegate<>;
	OnWindowClosed& OnClosed();

private:

	void InitializeWindow(std::string_view name, math::Vector2i resolution);

	std::unique_ptr<GLFWWindowData> m_windowData;
};

}

#endif // USE_GLFW
