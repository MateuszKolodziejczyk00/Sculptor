#if USE_WIN32
#include "Win32InputAdapter.h"
#include "SculptorCoreTypes.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>


namespace spt::platf
{

namespace priv
{

// EKey values are consecutive within each block, so checking its last key validates the whole block
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::RightMouseButton) == VK_RBUTTON);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::Mouse5)           == VK_XBUTTON2);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::Tab)              == VK_TAB);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::Enter)            == VK_RETURN);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::CapsLock)         == VK_CAPITAL);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::Esc)              == VK_ESCAPE);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::Down)             == VK_DOWN);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::Delete)           == VK_DELETE);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::Menu)             == VK_APPS);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::NumAdd)           == VK_ADD);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::NumDivide)        == VK_DIVIDE);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::F24)              == VK_F24);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::ScrollLock)       == VK_SCROLL);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::RAlt)             == VK_RMENU);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::GraveAccent)      == VK_OEM_3);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::Apostrophe)       == VK_OEM_7);
SPT_STATIC_CHECK(static_cast<Uint32>(inp::EKey::World1)           == VK_OEM_102);


static inp::EKey TranslateKeyboardInputCode(Uint32 virtualKey, Uint32 scanCode, Bool isExtended)
{
	switch (virtualKey)
	{
	case VK_SHIFT:
		return MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT ? inp::EKey::RShift : inp::EKey::LShift;
	case VK_CONTROL:					return isExtended ? inp::EKey::RCtrl    : inp::EKey::LCtrl;
	case VK_MENU:						return isExtended ? inp::EKey::RAlt     : inp::EKey::LAlt;
	case VK_RETURN:						return isExtended ? inp::EKey::NumEnter : inp::EKey::Enter;

	// Without the extended flag these come from the numpad (num lock off)
	case VK_INSERT:						return isExtended ? inp::EKey::Insert   : inp::EKey::Num0;
	case VK_DELETE:						return isExtended ? inp::EKey::Delete   : inp::EKey::NumDecimal;
	case VK_HOME:						return isExtended ? inp::EKey::Home     : inp::EKey::Num7;
	case VK_END:						return isExtended ? inp::EKey::End      : inp::EKey::Num1;
	case VK_PRIOR:						return isExtended ? inp::EKey::PageUp   : inp::EKey::Num9;
	case VK_NEXT:						return isExtended ? inp::EKey::PageDown : inp::EKey::Num3;
	case VK_UP:							return isExtended ? inp::EKey::Up       : inp::EKey::Num8;
	case VK_DOWN:						return isExtended ? inp::EKey::Down     : inp::EKey::Num2;
	case VK_LEFT:						return isExtended ? inp::EKey::Left     : inp::EKey::Num4;
	case VK_RIGHT:						return isExtended ? inp::EKey::Right    : inp::EKey::Num6;
	case VK_CLEAR:						return inp::EKey::Num5;
	}

	return static_cast<inp::EKey>(virtualKey);
}

} // priv

Win32InputAdapter::Win32InputAdapter() = default;

void Win32InputAdapter::PreUpdateInput()
{
	PreUpdateInputImpl();
}

void Win32InputAdapter::OnKeyboardMessage(Uint32 virtualKey, Uint32 scanCode, Bool isExtended, Bool isDown, Bool isRepeat)
{
	SPT_CHECK(virtualKey < static_cast<Uint32>(inp::EKey::Num));

	const inp::EKey key = priv::TranslateKeyboardInputCode(virtualKey, scanCode, isExtended);

	const inp::EInputActionType action = !isDown ? inp::EInputActionType::Release
											     : (isRepeat ? inp::EInputActionType::Repeat : inp::EInputActionType::Press);

	OnKeyActionImpl(key, action);
}

void Win32InputAdapter::OnMouseButtonMessage(Uint32 virtualKey, Bool isDown)
{
	OnKeyActionImpl(static_cast<inp::EKey>(virtualKey), isDown ? inp::EInputActionType::Press : inp::EInputActionType::Release);
}

void Win32InputAdapter::OnMousePositionChanged(Int32 newX, Int32 newY)
{
	OnMousePositionChangedImpl(math::Vector2i(newX, newY));
}

void Win32InputAdapter::OnScrollPositionChanged(Real32 delta)
{
	OnScrollPositionChangedImpl(delta);
}

} // spt::platf

#endif // USE_WIN32
