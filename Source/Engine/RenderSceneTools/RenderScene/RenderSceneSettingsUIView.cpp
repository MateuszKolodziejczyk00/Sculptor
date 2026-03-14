#include "RenderSceneSettingsUIView.h"
#include "RenderScene.h"
#include "ImGui/DockBuilder.h"
#include "SceneRenderer/SceneRenderer.h"
#include "UIElements/ApplicationUI.h"
#include "SceneRenderer/SceneRenderer.h"
#include "Engine.h"


namespace spt::rsc
{

RenderSceneSettingsUIView::RenderSceneSettingsUIView(const scui::ViewDefinition& definition, const lib::SharedPtr<RenderScene>& renderScene)
	: Super(definition)
	, m_renderScene(renderScene)
	, m_renderSceneSettingsName(CreateUniqueName("Render Scene"))
{ }

void RenderSceneSettingsUIView::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID, ui::DockWindow(m_renderSceneSettingsName));
}

void RenderSceneSettingsUIView::DrawUI()
{
	SPT_PROFILER_FUNCTION();
	
	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_renderSceneSettingsName.GetData());

	Super::DrawUI();

	if (auto scene = m_renderScene.lock())
	{
		DrawUIForScene(*scene);
	}

	ImGui::End();
}

void RenderSceneSettingsUIView::DrawUIForScene(RenderScene& scene)
{
	const scui::Context& context = scui::ApplicationUI::GetCurrentContext();

	const SceneRendererDLLModuleAPI* sceneRendererAPI = engn::Engine::Get().GetModulesManager().GetModuleAPI<SceneRendererDLLModuleAPI>();
	sceneRendererAPI->DrawParametersUI(context.GetUIContext().GetHandle());
}

} // spt::rsc
