#include "UIElements/UIWindow.h"
#include "ImGui/SculptorImGui.h"
#include "UIUtils.h"

namespace spt::scui
{

UIWindow::UIWindow(const lib::HashedString& name)
	: m_name(name)
{ }

const lib::HashedString& UIWindow::GetName() const
{
	return m_name;
}

void UIWindow::Draw(ui::UIContext context)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(context.IsValid());

	ImGui::SetCurrentContext(context.GetHandle());

	ImGui::Begin(m_name.GetData(), nullptr, ImGuiWindowFlags_NoDocking);

	ImGui::DockSpace(ImGui::GetID(m_name.GetData()), ui::UIUtils::GetWindowContentSize());

	m_layers.Draw();

	ImGui::End();
}

} // spt::scui
