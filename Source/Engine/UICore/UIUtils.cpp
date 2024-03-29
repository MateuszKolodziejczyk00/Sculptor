#include "UIUtils.h"
#include "ImGui/SculptorImGui.h"
#include "InputManager.h"

namespace spt::ui
{

math::Vector2f UIUtils::GetWindowContentSize()
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	const Real32 minX = window->Pos.x - window->Scroll.x + window->WindowPadding.x + window->DecoOuterSizeX1;
	const Real32 minY = window->Pos.y - window->Scroll.y + window->WindowPadding.y + window->DecoOuterSizeY1;
	const Real32 maxX = window->ContentRegionRect.Min.x + (window->ContentSizeExplicit.x != 0.0f ? window->ContentSizeExplicit.x : (window->SizeFull.x - window->WindowPadding.x * 2.0f - (window->DecoOuterSizeX1 + window->DecoOuterSizeX2)));
	const Real32 maxY = window->ContentRegionRect.Min.y + (window->ContentSizeExplicit.y != 0.0f ? window->ContentSizeExplicit.y : (window->SizeFull.y - window->WindowPadding.y * 2.0f - (window->DecoOuterSizeY1 + window->DecoOuterSizeY2)));
	return math::Vector2f(maxX - minX, maxY - minY);
}

void UIUtils::SetShortcut(ui::UIContext uiContext, ImGuiID itemID, const ShortcutBinding& binding)
{
	inp::InputManager& inputManager = inp::InputManager::Get();
	const Bool isShortcutPressed = std::all_of(binding.keys.cbegin(), binding.keys.cend(), [&inputManager](inp::EKey key) { return inputManager.IsKeyPressed(key); })
		                        && std::any_of(binding.keys.cbegin(), binding.keys.cend(), [&inputManager](inp::EKey key) { return inputManager.WasKeyJustPressed(key); });

	if (isShortcutPressed)
	{
		ImGui::ActivateItem(itemID);
		uiContext.GetHandle()->NavInputSource = ImGuiInputSource_Keyboard;
	}
}

} // spt::ui
