#include "UIElements/ApplicationUI.h"
#include "ImGui/SculptorImGui.h"
#include "UIUtils.h"

namespace spt::scui
{

ApplicationUI& ApplicationUI::GetInstance()
{
	static ApplicationUI instance;
	return instance;
}

void ApplicationUI::AddView(lib::SharedRef<UIView> view)
{
	ApplicationUI& instance = GetInstance();

	return instance.m_views.AddView(std::move(view));
}

void ApplicationUI::CloseView(const lib::SharedPtr<UIView>& view)
{
	ApplicationUI& instance = GetInstance();

	instance.m_views.RemoveView(view);
}

void ApplicationUI::CloseView(UIViewID id)
{
	ApplicationUI& instance = GetInstance();

	instance.m_views.RemoveView(id);
}

void ApplicationUI::CloseAllViews()
{
	ApplicationUI& instance = GetInstance();

	instance.m_views.ClearViews();
}

void ApplicationUI::Draw(ui::UIContext context)
{
	SPT_PROFILER_FUNCTION();

	ApplicationUI& instance = GetInstance();

	SPT_CHECK(context.IsValid());

	ImGui::SetCurrentContext(context.GetHandle());

	const ImGuiID mainDockspace = ImGui::GetID("ApplicationDockspace");

	ImGuiWindowClass mainWindowDockspaceClass;
	mainWindowDockspaceClass.ClassId = mainDockspace;
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), 0, &mainWindowDockspaceClass);

	UIViewDrawParams params;
	params.parentClass = mainWindowDockspaceClass;

	instance.m_views.DrawViews(params);
}

} // spt::scui