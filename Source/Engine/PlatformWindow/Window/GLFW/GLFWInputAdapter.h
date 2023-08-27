#pragma once

#if USE_GLFW

#include "InputAdapter.h"


namespace spt::platf
{

class GLFWInputAdapter : public inp::InputAdapter
{
public:

	GLFWInputAdapter();

	void PreUpdateInput();

	void OnMouseKeyAction(int key, int keyAction);
	void OnKeyboardKeyAction(int key, int keyAction);

	void OnMousePositionChanged(double newX, double newY);
	
	void OnScrollPositionChanged(double xOffset, double yOffset);
};

} // spt::platf

#endif // USE_GLFW
