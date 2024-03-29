#pragma once

#include "InputMacros.h"
#include "SculptorCoreTypes.h"
#include "InputTypes.h"


namespace spt::inp
{

class INPUT_API InputManager
{
public:

	static InputManager& Get();

	Bool IsKeyPressed(EKey key) const;
	Bool WasKeyJustPressed(EKey key) const;

	math::Vector2i GetMousePosition() const;
	math::Vector2i GetMouseMoveDelta() const;

	Real32 GetScrollDelta() const;

private:

	InputManager();

	void PreUpdateInput();

	void OnKeyAction(EKey key, EInputActionType action);
	void OnMousePositionChanged(const math::Vector2i& newPosition);

	void OnScrollPositionChanged(Real32 delta);

	static constexpr SizeType keysNum = static_cast<SizeType>(EKey::Num);

	struct KeyState
	{
		Bool m_isDown         = false;
		Bool m_wasJustPressed = false;
	};

	lib::StaticArray<KeyState, keysNum> m_keysPressStatus;

	math::Vector2i m_mouseCurrentPosition;
	math::Vector2i m_mousePrevPosition;

	Real32 m_scrollDelta;

	friend class InputAdapter;
};

} // spt::inp