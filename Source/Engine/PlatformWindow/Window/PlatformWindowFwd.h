#pragma once


#if USE_GLFW

namespace spt::platform
{
class GLFWWindow;

using PlatformWindow = GLFWWindow;

}

#endif // USE_GLFW