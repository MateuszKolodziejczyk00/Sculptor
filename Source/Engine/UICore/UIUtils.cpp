#include "UIUtils.h"
#include "ImGui/SculptorImGui.h"
#include "InputManager.h"

namespace spt::ui
{

namespace palette
{

static constexpr ImVec4 black       = ImVec4(0.000f, 0.000f, 0.000f, 1.f);
static constexpr ImVec4 surface     = ImVec4(0.047f, 0.047f, 0.051f, 1.f);
static constexpr ImVec4 frame       = ImVec4(0.078f, 0.078f, 0.086f, 1.f);
static constexpr ImVec4 frameHover  = ImVec4(0.110f, 0.110f, 0.122f, 1.f);
static constexpr ImVec4 frameActive = ImVec4(0.137f, 0.137f, 0.153f, 1.f);
static constexpr ImVec4 hairline    = ImVec4(0.102f, 0.102f, 0.110f, 1.f);
static constexpr ImVec4 muted       = ImVec4(0.230f, 0.230f, 0.250f, 1.f);
static constexpr ImVec4 text        = ImVec4(0.902f, 0.902f, 0.910f, 1.f);
static constexpr ImVec4 textDim     = ImVec4(0.560f, 0.560f, 0.590f, 1.f);
static constexpr ImVec4 accent      = ImVec4(0.541f, 0.706f, 1.000f, 1.f);
static constexpr ImVec4 accentLight = ImVec4(0.760f, 0.850f, 1.000f, 1.f);

static constexpr ImVec4 WithAlpha(ImVec4 color, Real32 alpha)
{
	color.w = alpha;
	return color;
}

} // palette

void UIUtils::InitializeStyle()
{
	using namespace palette;

	ImGuiStyle& style = ImGui::GetStyle();

	style.WindowPadding     = ImVec2(12.f, 12.f);
	style.FramePadding      = ImVec2(10.f, 5.f);
	style.CellPadding       = ImVec2(8.f, 5.f);
	style.ItemSpacing       = ImVec2(10.f, 8.f);
	style.ItemInnerSpacing  = ImVec2(8.f, 6.f);
	style.IndentSpacing     = 16.f;
	style.ScrollbarSize     = 8.f;
	style.GrabMinSize       = 4.f;

	style.WindowBorderSize  = 1.f;
	style.ChildBorderSize   = 1.f;
	style.PopupBorderSize   = 1.f;
	style.FrameBorderSize   = 0.f;
	style.TabBorderSize     = 0.f;

	style.WindowRounding    = 8.f;
	style.ChildRounding     = 6.f;
	style.FrameRounding     = 6.f;
	style.PopupRounding     = 6.f;
	style.ScrollbarRounding = 8.f;
	style.GrabRounding      = 4.f;
	style.TabRounding       = 6.f;

	style.WindowTitleAlign         = ImVec2(0.f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_None;

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Text]                  = text;
	colors[ImGuiCol_TextDisabled]          = textDim;
	colors[ImGuiCol_WindowBg]              = black;
	colors[ImGuiCol_ChildBg]               = WithAlpha(black, 0.f);
	colors[ImGuiCol_PopupBg]               = WithAlpha(surface, 0.98f);
	colors[ImGuiCol_Border]                = hairline;
	colors[ImGuiCol_BorderShadow]          = WithAlpha(black, 0.f);
	colors[ImGuiCol_FrameBg]               = frame;
	colors[ImGuiCol_FrameBgHovered]        = frameHover;
	colors[ImGuiCol_FrameBgActive]         = frameActive;
	colors[ImGuiCol_TitleBg]               = black;
	colors[ImGuiCol_TitleBgActive]         = black;
	colors[ImGuiCol_TitleBgCollapsed]      = black;
	colors[ImGuiCol_MenuBarBg]             = black;
	colors[ImGuiCol_ScrollbarBg]           = WithAlpha(black, 0.f);
	colors[ImGuiCol_ScrollbarGrab]         = frameActive;
	colors[ImGuiCol_ScrollbarGrabHovered]  = muted;
	colors[ImGuiCol_ScrollbarGrabActive]   = WithAlpha(accent, 0.8f);
	colors[ImGuiCol_CheckMark]             = accent;
	colors[ImGuiCol_SliderGrab]            = accent;
	colors[ImGuiCol_SliderGrabActive]      = accentLight;
	colors[ImGuiCol_Button]                = frame;
	colors[ImGuiCol_ButtonHovered]         = frameActive;
	colors[ImGuiCol_ButtonActive]          = WithAlpha(accent, 0.35f);
	colors[ImGuiCol_Header]                = WithAlpha(accent, 0.14f);
	colors[ImGuiCol_HeaderHovered]         = WithAlpha(text, 0.06f);
	colors[ImGuiCol_HeaderActive]          = WithAlpha(accent, 0.22f);
	colors[ImGuiCol_Separator]             = hairline;
	colors[ImGuiCol_SeparatorHovered]      = WithAlpha(accent, 0.5f);
	colors[ImGuiCol_SeparatorActive]       = accent;
	colors[ImGuiCol_ResizeGrip]            = WithAlpha(black, 0.f);
	colors[ImGuiCol_ResizeGripHovered]     = WithAlpha(accent, 0.4f);
	colors[ImGuiCol_ResizeGripActive]      = WithAlpha(accent, 0.8f);
	colors[ImGuiCol_Tab]                   = black;
	colors[ImGuiCol_TabHovered]            = frame;
	colors[ImGuiCol_TabActive]             = surface;
	colors[ImGuiCol_TabUnfocused]          = black;
	colors[ImGuiCol_TabUnfocusedActive]    = surface;
	colors[ImGuiCol_DockingPreview]        = WithAlpha(accent, 0.3f);
	colors[ImGuiCol_DockingEmptyBg]        = black;
	colors[ImGuiCol_PlotLines]             = accent;
	colors[ImGuiCol_PlotLinesHovered]      = accentLight;
	colors[ImGuiCol_PlotHistogram]         = accent;
	colors[ImGuiCol_PlotHistogramHovered]  = accentLight;
	colors[ImGuiCol_TableHeaderBg]         = surface;
	colors[ImGuiCol_TableBorderStrong]     = hairline;
	colors[ImGuiCol_TableBorderLight]      = hairline;
	colors[ImGuiCol_TableRowBg]            = WithAlpha(black, 0.f);
	colors[ImGuiCol_TableRowBgAlt]         = WithAlpha(text, 0.02f);
	colors[ImGuiCol_TextSelectedBg]        = WithAlpha(accent, 0.25f);
	colors[ImGuiCol_DragDropTarget]        = accent;
	colors[ImGuiCol_NavHighlight]          = WithAlpha(accent, 0.8f);
	colors[ImGuiCol_NavWindowingHighlight] = WithAlpha(text, 0.5f);
	colors[ImGuiCol_NavWindowingDimBg]     = WithAlpha(black, 0.6f);
	colors[ImGuiCol_ModalWindowDimBg]      = WithAlpha(black, 0.6f);
}

void UIUtils::DrawDockTabsAccent()
{
	const ImGuiContext& g = *ImGui::GetCurrentContext();

	for (const ImGuiStorage::ImGuiStoragePair& entry : g.DockContext.Nodes.Data)
	{
		// Removed dock nodes are kept in storage as null entries
		const ImGuiDockNode* node = static_cast<const ImGuiDockNode*>(entry.val_p);
		if (!node || !node->IsVisible || !node->TabBar || !node->HostWindow || node->IsHiddenTabBar() || node->IsNoTabBar())
		{
			continue;
		}

		ImGuiTabBar& tabBar = *node->TabBar;
		const ImGuiTabItem* tab = ImGui::TabBarFindTabByID(&tabBar, tabBar.VisibleTabId);
		if (!tab || tab->LastFrameVisible != g.FrameCount)
		{
			continue;
		}

		// Matches the tab layout in ImGui::TabItemEx
		const Real32 tabMinX = tabBar.BarRect.Min.x + IM_FLOOR(tab->Offset - tabBar.ScrollingAnim);
		const Real32 inset   = g.Style.TabRounding;

		ImDrawList* drawList = node->HostWindow->DrawList;
		drawList->PushClipRect(tabBar.BarRect.Min, tabBar.BarRect.Max, false);
		drawList->AddRectFilled(ImVec2(tabMinX + inset, tabBar.BarRect.Max.y - 2.f),
								ImVec2(tabMinX + tab->Width - inset, tabBar.BarRect.Max.y),
								ImGui::GetColorU32(node->IsFocused ? palette::accent : palette::muted),
								1.f);
		drawList->PopClipRect();
	}
}

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
