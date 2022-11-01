#include "UIElements/UIWindow.h"
#include "ImGui/SculptorImGui.h"
#include "UIUtils.h"

namespace spt::scui
{

UIWindow::UIWindow(const lib::HashedString& name)
	: m_name(name)
	, bWantsClose(false)
{ }

const lib::HashedString& UIWindow::GetName() const
{
	return m_name;
}

Bool UIWindow::WantsClose() const
{
	return bWantsClose;
}

void UIWindow::DrawWindow()
{
	SPT_PROFILER_FUNCTION();

	Bool isOpen = true;
	ImGui::Begin(m_name.GetData(), &isOpen, ImGuiWindowFlags_NoDocking);

	ImGui::DockSpace(ImGui::GetID(m_name.GetData()), ui::UIUtils::GetWindowContentSize());

	DrawContent();

	ImGui::End();

	bWantsClose = !isOpen;
}

void UIWindow::DrawContent()
{
	SPT_PROFILER_FUNCTION();

	m_layers.Draw();
}

} // spt::scui
