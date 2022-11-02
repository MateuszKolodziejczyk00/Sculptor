#include "UIElements/UIWindow.h"
#include "ImGui/SculptorImGui.h"
#include "UIUtils.h"

namespace spt::scui
{

const ImGuiID& UIWindow::GetWindowsDockspaceID()
{
	static constexpr ImGuiID id = 1;
	return id;
}

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

	ImGuiWindowClass windowClass;
	windowClass.ClassId = GetWindowsDockspaceID();
	windowClass.DockNodeFlagsOverrideSet = /*ImGuiDockNodeFlags_NoDockingOverMe | */ImGuiDockNodeFlags_NoDockingSplitMe;

	ImGui::SetNextWindowClass(&windowClass);

	const WindowBuildingScope scope(GetName());

	Bool isOpen = true;
	ImGui::Begin(m_name.GetData(), &isOpen);

	ImGui::DockSpace(ImGui::GetID(m_name.GetData()), ui::UIUtils::GetWindowContentSize());

	m_layers.Draw();

	ImGui::End();

	bWantsClose = !isOpen;
}

} // spt::scui
