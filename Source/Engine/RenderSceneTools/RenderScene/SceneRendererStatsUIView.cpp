#include "SceneRendererStatsUIView.h"
#include "SceneRenderer/Debug/Stats/SceneRendererStatsView.h"
#include "ImGui/DockBuilder.h"


namespace spt::rsc
{

SceneRendererStatsUIView::SceneRendererStatsUIView(const scui::ViewDefinition& definition)
	: Super(definition)
	, m_statsPanelName("StatsPanel")
{
}

void SceneRendererStatsUIView::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID,
			  ui::DockWindow(m_statsPanelName));
}

void SceneRendererStatsUIView::DrawUI()
{
	Super::DrawUI();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_statsPanelName.GetData());

	SceneRendererStatsRegistry::GetInstance().DrawUI();

	ImGui::End();
}

} // spt::rsc
