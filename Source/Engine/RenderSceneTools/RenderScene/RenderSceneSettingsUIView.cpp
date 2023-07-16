#include "RenderSceneSettingsUIView.h"
#include "RenderScene.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"
#include "DDGI/DDGISceneSubsystem.h"

namespace spt::rsc
{

RenderSceneSettingsUIView::RenderSceneSettingsUIView(const scui::ViewDefinition& definition, const lib::SharedPtr<RenderScene>& renderScene)
	: Super(definition)
	, m_renderScene(renderScene)
{ }

void RenderSceneSettingsUIView::DrawUI()
{
	SPT_PROFILER_FUNCTION();
	
	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin("Render Scene");

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

	if (lib::SharedPtr<rsc::ShadowMapsManagerSubsystem> shadowMapsManger = scene.GetSceneSubsystem<rsc::ShadowMapsManagerSubsystem>())
	{
		const char* shadowMappingTechniques[3] = { "None", "DPCF", "MSM" };

		int currentTechnique = static_cast<int>(shadowMapsManger->GetShadowMappingTechnique());
		if (ImGui::Combo("Point Lights Shadow Mapping Technique", &currentTechnique, shadowMappingTechniques, SPT_ARRAY_SIZE(shadowMappingTechniques)))
		{
			shadowMapsManger->SetShadowMappingTechnique(static_cast<rsc::EShadowMappingTechnique>(currentTechnique));
		}
	}

	if (lib::SharedPtr<rsc::DDGISceneSubsystem> ddgiSubsystem = scene.GetSceneSubsystem<rsc::DDGISceneSubsystem>())
	{
		const char* ddgiDebugModes[rsc::EDDDGIProbesDebugMode::NUM] = { "None", "Illuminance", "Hit Distance" };
		int ddgiDebugMode = ddgiSubsystem->GetProbesDebugMode();
		if (ImGui::Combo("DDGI Probes Visualization", &ddgiDebugMode, ddgiDebugModes, SPT_ARRAY_SIZE(ddgiDebugModes)))
		{
			ddgiSubsystem->SetProbesDebugMode(static_cast<rsc::EDDDGIProbesDebugMode::Type>(ddgiDebugMode));
		}
	}
}

} // spt::rsc
