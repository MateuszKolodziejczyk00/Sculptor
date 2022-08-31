#pragma once


#if USE_GLFW

namespace spt::platf
{

class GLFWWindow;

using PlatformWindow = GLFWWindow;


} // spt::plat

#endif // USE_GLFW