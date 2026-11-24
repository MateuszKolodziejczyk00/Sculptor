#pragma once

#if USE_WIN32

#include "InputAdapter.h"


namespace spt::platf
{

class Win32InputAdapter : public inp::InputAdapter
{
public:

	Win32InputAdapter();

	void PreUpdateInput();

	void OnKeyboardMessage(Uint32 virtualKey, Uint32 scanCode, Bool isExtended, Bool isDown, Bool isRepeat);

	void OnMouseButtonMessage(Uint32 virtualKey, Bool isDown);

	void OnMousePositionChanged(Int32 newX, Int32 newY);

	void OnScrollPositionChanged(Real32 delta);
};

} // spt::platf

#endif // USE_WIN32
