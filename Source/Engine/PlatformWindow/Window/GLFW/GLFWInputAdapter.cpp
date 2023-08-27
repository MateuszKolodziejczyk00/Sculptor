#if USE_GLFW
#include "GLFWInputAdapter.h"
#include "SculptorCoreTypes.h"

#include "GLFW/glfw3.h"

namespace spt::platf
{

namespace priv
{

inp::EKey TranslateKeyboardInputCode(int key)
{
	switch (key)
	{
	case GLFW_KEY_SPACE:				return inp::EKey::Space;
	case GLFW_KEY_APOSTROPHE:			return inp::EKey::Apostrophe;
	case GLFW_KEY_COMMA:				return inp::EKey::Comma;
	case GLFW_KEY_MINUS:				return inp::EKey::Minus;
	case GLFW_KEY_PERIOD:				return inp::EKey::Period;
	case GLFW_KEY_SLASH:				return inp::EKey::Slash;
	case GLFW_KEY_0:					return inp::EKey::_0;
	case GLFW_KEY_1:					return inp::EKey::_1;
	case GLFW_KEY_2:					return inp::EKey::_2;
	case GLFW_KEY_3:					return inp::EKey::_3;
	case GLFW_KEY_4:					return inp::EKey::_4;
	case GLFW_KEY_5:					return inp::EKey::_5;
	case GLFW_KEY_6:					return inp::EKey::_6;
	case GLFW_KEY_7:					return inp::EKey::_7;
	case GLFW_KEY_8:					return inp::EKey::_8;
	case GLFW_KEY_9:					return inp::EKey::_9;
	case GLFW_KEY_SEMICOLON:			return inp::EKey::Semicolon;
	case GLFW_KEY_EQUAL:				return inp::EKey::Equal;
	case GLFW_KEY_A:					return inp::EKey::A;
	case GLFW_KEY_B:					return inp::EKey::B;
	case GLFW_KEY_C:					return inp::EKey::C;
	case GLFW_KEY_D:					return inp::EKey::D;
	case GLFW_KEY_E:					return inp::EKey::E;
	case GLFW_KEY_F:					return inp::EKey::F;
	case GLFW_KEY_G:					return inp::EKey::G;
	case GLFW_KEY_H:					return inp::EKey::H;
	case GLFW_KEY_I:					return inp::EKey::I;
	case GLFW_KEY_J:					return inp::EKey::J;
	case GLFW_KEY_K:					return inp::EKey::K;
	case GLFW_KEY_L:					return inp::EKey::L;
	case GLFW_KEY_M:					return inp::EKey::M;
	case GLFW_KEY_N:					return inp::EKey::N;
	case GLFW_KEY_O:					return inp::EKey::O;
	case GLFW_KEY_P:					return inp::EKey::P;
	case GLFW_KEY_Q:					return inp::EKey::Q;
	case GLFW_KEY_R:					return inp::EKey::R;
	case GLFW_KEY_S:					return inp::EKey::S;
	case GLFW_KEY_T:					return inp::EKey::T;
	case GLFW_KEY_U:					return inp::EKey::U;
	case GLFW_KEY_V:					return inp::EKey::V;
	case GLFW_KEY_W:					return inp::EKey::W;
	case GLFW_KEY_X:					return inp::EKey::X;
	case GLFW_KEY_Y:					return inp::EKey::Y;
	case GLFW_KEY_Z:					return inp::EKey::Z;
	case GLFW_KEY_LEFT_BRACKET:			return inp::EKey::LeftBracket;
	case GLFW_KEY_BACKSLASH:			return inp::EKey::BackSlash;
	case GLFW_KEY_RIGHT_BRACKET:		return inp::EKey::RightBracket;
	case GLFW_KEY_GRAVE_ACCENT:			return inp::EKey::GraveAccent;
	case GLFW_KEY_WORLD_1:				return inp::EKey::World1;
	case GLFW_KEY_WORLD_2:				return inp::EKey::World2;
	case GLFW_KEY_ESCAPE:				return inp::EKey::Esc;
	case GLFW_KEY_ENTER:				return inp::EKey::Enter;
	case GLFW_KEY_TAB:					return inp::EKey::Tab;
	case GLFW_KEY_BACKSPACE:			return inp::EKey::Backspace;
	case GLFW_KEY_INSERT:				return inp::EKey::Insert;
	case GLFW_KEY_DELETE:				return inp::EKey::Delete;
	case GLFW_KEY_RIGHT:				return inp::EKey::Right;
	case GLFW_KEY_LEFT:					return inp::EKey::Left;
	case GLFW_KEY_DOWN:					return inp::EKey::Down;
	case GLFW_KEY_UP:					return inp::EKey::Up;
	case GLFW_KEY_PAGE_UP:				return inp::EKey::PageUp;
	case GLFW_KEY_PAGE_DOWN :			return inp::EKey::PageDown;
	case GLFW_KEY_HOME:					return inp::EKey::Home;
	case GLFW_KEY_END:					return inp::EKey::End;
	case GLFW_KEY_CAPS_LOCK:			return inp::EKey::CapsLock;
	case GLFW_KEY_SCROLL_LOCK:			return inp::EKey::ScrollLock;
	case GLFW_KEY_NUM_LOCK:				return inp::EKey::NumLock;
	case GLFW_KEY_PRINT_SCREEN:			return inp::EKey::PrintScreen;
	case GLFW_KEY_PAUSE:				return inp::EKey::Pause;
	case GLFW_KEY_F1:					return inp::EKey::F1;
	case GLFW_KEY_F2:					return inp::EKey::F2;
	case GLFW_KEY_F3:					return inp::EKey::F3;
	case GLFW_KEY_F4:					return inp::EKey::F4;
	case GLFW_KEY_F5:					return inp::EKey::F5;
	case GLFW_KEY_F6:					return inp::EKey::F6;
	case GLFW_KEY_F7:					return inp::EKey::F7;
	case GLFW_KEY_F8:					return inp::EKey::F8;
	case GLFW_KEY_F9:					return inp::EKey::F9;
	case GLFW_KEY_F10:					return inp::EKey::F10;
	case GLFW_KEY_F11:					return inp::EKey::F11;
	case GLFW_KEY_F12:					return inp::EKey::F12;
	case GLFW_KEY_F13:					return inp::EKey::F13;
	case GLFW_KEY_F14:					return inp::EKey::F14;
	case GLFW_KEY_F15:					return inp::EKey::F15;
	case GLFW_KEY_F16:					return inp::EKey::F16;
	case GLFW_KEY_F17:					return inp::EKey::F17;
	case GLFW_KEY_F18:					return inp::EKey::F18;
	case GLFW_KEY_F19:					return inp::EKey::F19;
	case GLFW_KEY_F20:					return inp::EKey::F20;
	case GLFW_KEY_F21:					return inp::EKey::F21;
	case GLFW_KEY_F22:					return inp::EKey::F22;
	case GLFW_KEY_F23:					return inp::EKey::F23;
	case GLFW_KEY_F24:					return inp::EKey::F24;
	case GLFW_KEY_F25:					return inp::EKey::F25;
	case GLFW_KEY_KP_0:					return inp::EKey::Num0;
	case GLFW_KEY_KP_1:					return inp::EKey::Num1;
	case GLFW_KEY_KP_2:					return inp::EKey::Num2;
	case GLFW_KEY_KP_3:					return inp::EKey::Num3;
	case GLFW_KEY_KP_4:					return inp::EKey::Num4;
	case GLFW_KEY_KP_5:					return inp::EKey::Num5;
	case GLFW_KEY_KP_6:					return inp::EKey::Num6;
	case GLFW_KEY_KP_7:					return inp::EKey::Num7;
	case GLFW_KEY_KP_8:					return inp::EKey::Num8;
	case GLFW_KEY_KP_9:					return inp::EKey::Num9;
	case GLFW_KEY_KP_DECIMAL:			return inp::EKey::NumDecimal;
	case GLFW_KEY_KP_DIVIDE:			return inp::EKey::NumDivide;
	case GLFW_KEY_KP_MULTIPLY:			return inp::EKey::NumMultiply;
	case GLFW_KEY_KP_SUBTRACT:			return inp::EKey::NumSubtract;
	case GLFW_KEY_KP_ADD:				return inp::EKey::NumAdd;
	case GLFW_KEY_KP_ENTER:				return inp::EKey::NumEnter;
	case GLFW_KEY_KP_EQUAL:				return inp::EKey::NumEqual;
	case GLFW_KEY_LEFT_SHIFT:			return inp::EKey::LShift;
	case GLFW_KEY_LEFT_CONTROL:			return inp::EKey::LCtrl;
	case GLFW_KEY_LEFT_ALT:				return inp::EKey::LAlt;
	case GLFW_KEY_LEFT_SUPER:			return inp::EKey::LSuper;
	case GLFW_KEY_RIGHT_SHIFT:			return inp::EKey::RShift;
	case GLFW_KEY_RIGHT_CONTROL:		return inp::EKey::RCtrl;
	case GLFW_KEY_RIGHT_ALT:			return inp::EKey::RAlt;
	case GLFW_KEY_RIGHT_SUPER:			return inp::EKey::RSuper;
	case GLFW_KEY_MENU:					return inp::EKey::Menu;
	}

	return inp::EKey::None;
}

inp::EKey TranslateMouseInputCode(int key)
{
	switch (key)
	{
	case GLFW_MOUSE_BUTTON_4:			return inp::EKey::Mouse4;
	case GLFW_MOUSE_BUTTON_5:			return inp::EKey::Mouse5;
	case GLFW_MOUSE_BUTTON_6:			return inp::EKey::Mouse6;
	case GLFW_MOUSE_BUTTON_7:			return inp::EKey::Mouse7;
	case GLFW_MOUSE_BUTTON_8:			return inp::EKey::Mouse8;
	case GLFW_MOUSE_BUTTON_LEFT:		return inp::EKey::LeftMouseButton;
	case GLFW_MOUSE_BUTTON_RIGHT:		return inp::EKey::RightMouseButton;
	case GLFW_MOUSE_BUTTON_MIDDLE:		return inp::EKey::MouseScroll;
	}

	return inp::EKey::None;
}

inp::EInputActionType TranslateInputAction(int action)
{
	switch (action)
	{
	case GLFW_PRESS:	return inp::EInputActionType::Press;
	case GLFW_REPEAT:	return inp::EInputActionType::Repeat;
	case GLFW_RELEASE:	return inp::EInputActionType::Release;
	}

	SPT_CHECK_NO_ENTRY();
	return inp::EInputActionType::Press;
}

} // priv

GLFWInputAdapter::GLFWInputAdapter() = default;

void GLFWInputAdapter::PreUpdateInput()
{
	PreUpdateInputImpl();
}

void GLFWInputAdapter::OnMouseKeyAction(int key, int keyAction)
{
	OnKeyActionImpl(priv::TranslateMouseInputCode(key), priv::TranslateInputAction(keyAction));
}

void GLFWInputAdapter::OnKeyboardKeyAction(int key, int keyAction)
{
	OnKeyActionImpl(priv::TranslateKeyboardInputCode(key), priv::TranslateInputAction(keyAction));
}

void GLFWInputAdapter::OnMousePositionChanged(double newX, double newY)
{
	OnMousePositionChangedImpl(math::Vector2i(static_cast<Int32>(newX), static_cast<Int32>(newY)));
}

void GLFWInputAdapter::OnScrollPositionChanged(double xOffset, double yOffset)
{
	OnScrollPositionChangedImpl(static_cast<Real32>(yOffset));
}

} // spt::platf

#endif // USE_GLFW
