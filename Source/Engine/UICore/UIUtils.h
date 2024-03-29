#pragma once

#include "UICoreMacros.h"
#include "SculptorCoreTypes.h"
#include "InputTypes.h"
#include "UIContext.h"


namespace spt::ui
{

struct ShortcutBinding
{
	static ShortcutBinding Create(auto... inKeys)
	{
		ShortcutBinding binding;
		binding.keys = { inKeys... };
		return binding;
	}

	lib::DynamicArray<inp::EKey> keys;
};


class UI_CORE_API UIUtils
{
public:

	static math::Vector2f GetWindowContentSize();

	static void SetShortcut(ui::UIContext uiContext, ImGuiID itemID, const ShortcutBinding& binding);
};

} // spt::ui