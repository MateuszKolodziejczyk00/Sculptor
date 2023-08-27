#include "UIView.h"
#include "ImGui/SculptorImGui.h"
#include "UIUtils.h"
#include "ImGui/DockBuilder.h"

namespace spt::scui
{

UIView::UIView(const ViewDefinition& definition)
	: m_id(UIViewID::GenerateID())
	, m_minimumSize(definition.minimumSize.cast<Real32>())
{
	m_name = CreateUniqueName(definition.name);
}

UIViewID UIView::GetID() const
{
	return m_id;
}

EViewDrawResultActions UIView::Draw(const UIViewDrawParams& params)
{
	SPT_PROFILER_SCOPE(GetName().GetData());

	// We want to be able to dock this view only to direct parent view
	// Content of this view can be docked only to this view window
	// To achieve this we create "master" window of this view which has same class as parent dockspace (so it can be docked to it)
	// Next we create another class (called ContentClass) for this views dockspace, content of this view and for children views
	// Content of this view should use same class as dockspace (ContentClass) so it can be docked to it

	ImGui::SetNextWindowClass(&params.parentClass);

	Bool isOpen = true;

	// "Master" window of this view
	if(ImGui::Begin(m_name.GetData(), &isOpen))
	{
		const math::Vector2f windowSize = m_minimumSize.cwiseMax(math::Vector2f(ImGui::GetWindowSize()));
		ImGui::SetWindowSize(windowSize);

		const ImGuiID dockspaceID = ImGui::GetID(m_name.GetData());

		ImGui::PushID(dockspaceID);

		ImGuiWindowClass contentClass;
		contentClass.ClassId = dockspaceID;

		ImGui::DockSpace(dockspaceID, ui::UIUtils::GetWindowContentSize(), 0, &contentClass);

		const ViewBuildingScope scope(contentClass);

		UIViewDrawParams childDrawParams;
		childDrawParams.parentClass = contentClass;

		m_children.DrawViews(childDrawParams);

		if (ui::ShouldBuildDock(dockspaceID))
		{
			BuildDefaultLayout(dockspaceID);
		}

		DrawUI();

		ImGui::PopID();
	}
	
	ImGui::End();

	return isOpen ? EViewDrawResultActions::None : EViewDrawResultActions::Close;
}

const lib::HashedString& UIView::GetName() const
{
	return m_name;
}

const lib::HashedString& UIView::AddChild(lib::SharedRef<UIView> child)
{
	const lib::HashedString& name = child->GetName();
	m_children.AddView(std::move(child));
	return name;
}

void UIView::RemoveChild(UIViewID viewID)
{
	m_children.RemoveView(viewID);
}

void UIView::RemoveChild(const lib::SharedPtr<UIView>& view)
{
	m_children.RemoveView(view);
}

void UIView::BuildDefaultLayout(ImGuiID dockspaceID) const
{

}

void UIView::DrawUI()
{

}

lib::HashedString UIView::CreateUniqueName(const lib::HashedString& name)
{
	return name.ToString() + "##" + std::to_string(UIViewID::GenerateID().GetValue());
}

} // spt::scui
