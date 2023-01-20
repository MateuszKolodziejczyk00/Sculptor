#include "InputAdapter.h"
#include "InputManager.h"

namespace spt::inp
{

void InputAdapter::PreUpdateInputImpl()
{
	InputManager::Get().PreUpdateInput();
}

void InputAdapter::OnKeyActionImpl(EKey key, EInputActionType action)
{
	InputManager::Get().OnKeyAction(key, action);
}

void InputAdapter::OnMousePositionChangedImpl(const math::Vector2i& newPosition)
{
	InputManager::Get().OnMousePositionChanged(newPosition);
}

} // spt::inp
