#pragma once


#if USE_GLFW

namespace spt::window
{
class GLFWWindow;

using Window = GLFWWindow;

}

#endif // USE_GLFW