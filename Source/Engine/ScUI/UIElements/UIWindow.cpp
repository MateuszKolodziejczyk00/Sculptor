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

void UIWindow::Draw()
{
	SPT_PROFILER_FUNCTION();

	Bool isOpen = true;
	ImGui::Begin(m_name.GetData(), &isOpen, ImGuiWindowFlags_NoDocking);

	ImGui::DockSpace(ImGui::GetID(m_name.GetData()), ui::UIUtils::GetWindowContentSize());

	m_layers.Draw();

	ImGui::End();

	bWantsClose = !isOpen;
}

} // spt::scui
