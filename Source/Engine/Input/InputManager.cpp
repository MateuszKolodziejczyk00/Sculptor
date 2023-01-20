#include "InputManager.h"

namespace spt::inp
{

InputManager& InputManager::Get()
{
	static InputManager instance;
	return instance;
}

Bool InputManager::IsKeyPressed(EKey key) const
{
	const SizeType keyIdx = static_cast<SizeType>(key);
	return m_keysPressStatus[keyIdx];
}

math::Vector2i InputManager::GetMousePosition() const
{
	return m_mouseCurrentPosition;
}

math::Vector2i InputManager::GetMouseMoveDelta() const
{
	return m_mouseCurrentPosition - m_mousePrevPosition;
}

InputManager::InputManager()
	: m_mouseCurrentPosition(math::Vector2i::Zero())
	, m_mousePrevPosition(math::Vector2i::Zero())
{
	std::fill(std::begin(m_keysPressStatus), std::end(m_keysPressStatus), false);
}

void InputManager::PreUpdateInput()
{
	m_mousePrevPosition = m_mouseCurrentPosition;
}

void InputManager::OnKeyAction(EKey key, EInputActionType action)
{
	const SizeType keyIdx = static_cast<SizeType>(key);
	m_keysPressStatus[keyIdx] = action != EInputActionType::Release;
}

void InputManager::OnMousePositionChanged(const math::Vector2i& newPosition)
{
	m_mouseCurrentPosition = newPosition;
}

} // spt::inp
