#pragma once

namespace spt::inp
{

// Matches Win32 virtual-key codes
enum class EKey
{
	None = 0,

	LeftMouseButton = 0x01,
	RightMouseButton,

	MouseScroll = 0x04,
	Mouse4,
	Mouse5,

	Backspace = 0x08,
	Tab,

	Enter = 0x0D,
	// Shares VK_RETURN with Enter (differs by extended flag), so it uses unassigned code
	NumEnter = 0x0E,

	Pause = 0x13,
	CapsLock,

	Esc = 0x1B,

	Space = 0x20,
	PageUp,
	PageDown,
	End,
	Home,
	Left,
	Up,
	Right,
	Down,

	PrintScreen = 0x2C,
	Insert,
	Delete,

	_0 = '0',
	_1,
	_2,
	_3,
	_4,
	_5,
	_6,
	_7,
	_8,
	_9,

	A = 'A',
	B,
	C,
	D,
	E,
	F,
	G,
	H,
	I,
	J,
	K,
	L,
	M,
	N,
	O,
	P,
	Q,
	R,
	S,
	T,
	U,
	V,
	W,
	X,
	Y,
	Z,

	LSuper = 0x5B,
	RSuper,
	Menu,

	Num0 = 0x60,
	Num1,
	Num2,
	Num3,
	Num4,
	Num5,
	Num6,
	Num7,
	Num8,
	Num9,
	NumMultiply,
	NumAdd,

	NumSubtract = 0x6D,
	NumDecimal,
	NumDivide,

	F1 = 0x70,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	F11,
	F12,
	F13,
	F14,
	F15,
	F16,
	F17,
	F18,
	F19,
	F20,
	F21,
	F22,
	F23,
	F24,

	NumLock = 0x90,
	ScrollLock,

	LShift = 0xA0,
	RShift,
	LCtrl,
	RCtrl,
	LAlt,
	RAlt,

	Semicolon = 0xBA,
	Equal,
	Comma,
	Minus,
	Period,
	Slash,
	GraveAccent,

	LeftBracket = 0xDB,
	BackSlash,
	RightBracket,
	Apostrophe,

	World1 = 0xE2,

	Num = 0x100
};


enum class EInputActionType
{
	Press,
	Repeat,
	Release
};

} // spt::inp
