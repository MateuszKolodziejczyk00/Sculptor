#pragma once

#include "InputMacros.h"
#include "InputTypes.h"
#include "MathCore.h"


namespace spt::inp
{

class INPUT_API InputAdapter abstract
{
public:

	InputAdapter() = default;
	virtual ~InputAdapter() = default;

protected:

	void PreUpdateInputImpl();

	void OnKeyActionImpl(EKey key, EInputActionType action);
	void OnMousePositionChangedImpl(const math::Vector2i& newPosition);
};

} // spt::inp