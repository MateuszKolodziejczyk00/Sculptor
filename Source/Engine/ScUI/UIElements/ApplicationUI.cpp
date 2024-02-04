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

void ApplicationUI::Draw(const Context& context)
{
	SPT_PROFILER_FUNCTION();

	ApplicationUI& instance = GetInstance();

	instance.m_currentContext = &context;

	SPT_CHECK(context.GetUIContext().IsValid());

	ImGui::SetCurrentContext(context.GetUIContext().GetHandle());

	const ImGuiID mainDockspace = ImGui::GetID("ApplicationDockspace");

	ImGuiWindowClass mainWindowDockspaceClass;
	mainWindowDockspaceClass.ClassId = mainDockspace;
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), 0, &mainWindowDockspaceClass);

	UIViewDrawParams params;
	params.parentClass = mainWindowDockspaceClass;

	instance.m_views.DrawViews(params);

	instance.m_currentContext = nullptr;
}

const Context& ApplicationUI::GetCurrentContext()
{
	ApplicationUI& instance = GetInstance();

	SPT_CHECK(instance.m_currentContext != nullptr);

	return *instance.m_currentContext;
}

} // spt::scui