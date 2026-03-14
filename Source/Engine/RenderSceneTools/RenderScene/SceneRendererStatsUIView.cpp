#include "SceneRendererStatsUIView.h"
#include "SceneRenderer/SceneRenderer.h"
#include "ImGui/DockBuilder.h"
#include "UIElements/ApplicationUI.h"
#include "SceneRenderer/SceneRenderer.h"
#include "Engine.h"


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
	const scui::Context& context = scui::ApplicationUI::GetCurrentContext();

	Super::DrawUI();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_statsPanelName.GetData());

	const SceneRendererDLLModuleAPI* sceneRendererAPI = engn::Engine::Get().GetModulesManager().GetModuleAPI<SceneRendererDLLModuleAPI>();
	sceneRendererAPI->DrawRendererStatsUI(context.GetUIContext().GetHandle());

	ImGui::End();
}

} // spt::rsc
