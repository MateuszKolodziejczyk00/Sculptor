#pragma once

#if USE_GLFW

#include "InputAdapter.h"


namespace spt::platf
{

class GLFWInputAdapter : public inp::InputAdapter
{
public:

	GLFWInputAdapter();

	void OnMouseKeyAction(int key, int keyAction);
	void OnKeyboardKeyAction(int key, int keyAction);

	void OnMousePositionChanged(double newX, double newY);
};

} // spt::platf

#endif // USE_GLFW
