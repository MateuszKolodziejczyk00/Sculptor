#include "RenderSceneSettingsUIView.h"
#include "RenderScene.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "Shadows/ShadowMapsRenderSystem.h"
#include "ImGui/DockBuilder.h"


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
	rsc::RendererParamsRegistry::DrawParametersUI();

	if (lib::SharedPtr<rsc::ShadowMapsRenderSystem> shadowMapsManger = scene.FindRenderSystem<rsc::ShadowMapsRenderSystem>())
	{
		const char* shadowMappingTechniques[3] = { "None", "DPCF", "MSM" };

		int currentTechnique = static_cast<int>(shadowMapsManger->GetShadowMappingTechnique());
		if (ImGui::Combo("Point Lights Shadow Mapping Technique", &currentTechnique, shadowMappingTechniques, SPT_ARRAY_SIZE(shadowMappingTechniques)))
		{
			shadowMapsManger->SetShadowMappingTechnique(static_cast<rsc::EShadowMappingTechnique>(currentTechnique));
		}
	}
}

} // spt::rsc
