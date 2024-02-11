#pragma once

#include "SculptorCoreTypes.h"
#include "SculptorImGui.h"


namespace spt::ui
{

class DockStack
{
public:

	explicit DockStack()
		: m_id(0)
	{ }

	void Execute(ImGuiID parent)
	{
		m_id = parent;
	}

	void Push(const lib::HashedString& name)
	{
		ImGuiContext& g = *GImGui;
		const ImGuiDockNode* dockNode = ImGui::DockContextFindNodeByID(&g, m_id);
		while (dockNode->IsSplitNode())
		{
			dockNode = dockNode->ChildNodes[0];
		}
		ImGui::DockBuilderDockWindow(name.GetData(), dockNode->ID);
	}

private:

	ImGuiID m_id;
};

} // namespace spt::ui