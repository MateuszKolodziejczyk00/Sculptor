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

lib::SharedRef<UIWindow> ApplicationUI::OpenWindow(const lib::HashedString& name)
{
	SPT_PROFILER_FUNCTION();

	ApplicationUI& instance = GetInstance();

	lib::SharedRef<UIWindow> newWindow = lib::MakeShared<UIWindow>(name);
	instance.m_windows.emplace_back(newWindow);

	return newWindow;
}

void ApplicationUI::CloseWindow(const lib::HashedString& name)
{
	SPT_PROFILER_FUNCTION();

	ApplicationUI& instance = GetInstance();

	const auto foundWindow = std::find_if(std::cbegin(instance.m_windows), std::cend(instance.m_windows),
										  [name](const lib::SharedPtr<UIWindow>& window)
										  {
											  return window->GetName() == name;
										  });

	if (foundWindow != std::cend(instance.m_windows))
	{
		instance.m_windows.erase(foundWindow);
	}
}

void ApplicationUI::Draw(ui::UIContext context)
{
	SPT_PROFILER_FUNCTION();

	ApplicationUI& instance = GetInstance();

	SPT_CHECK(context.IsValid());

	// create copy, in case of adding new windows during draw
	lib::DynamicArray<lib::SharedPtr<UIWindow>> windowsCopy = std::move(instance.m_windows);

	ImGui::SetCurrentContext(context.GetHandle());

	ImGuiWindowClass mainWindowDockspaceClass;
	mainWindowDockspaceClass.ClassId = UIWindow::GetWindowsDockspaceID();
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), 0, &mainWindowDockspaceClass);
	
	for (const lib::SharedPtr<UIWindow>& window : windowsCopy)
	{
		window->Draw();
	}

	if (instance.m_windows.empty())
	{
		instance.m_windows = std::move(windowsCopy);
	}
	else
	{
		instance.m_windows.reserve(instance.m_windows.size() + windowsCopy.size());

		std::copy_if(std::cbegin(windowsCopy), std::cend(windowsCopy),
					 std::back_inserter(instance.m_windows),
					 [](const lib::SharedPtr<UIWindow>& window)
					 {
						 return window->WantsClose();
					 });
	}
}

} // spt::scui