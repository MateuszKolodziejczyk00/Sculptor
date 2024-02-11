#include "RenderSceneSettingsUIView.h"
#include "RenderScene.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"
#include "DDGI/DDGISceneSubsystem.h"
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

	if (lib::SharedPtr<rsc::ShadowMapsManagerSubsystem> shadowMapsManger = scene.GetSceneSubsystem<rsc::ShadowMapsManagerSubsystem>())
	{
		const char* shadowMappingTechniques[3] = { "None", "DPCF", "MSM" };

		int currentTechnique = static_cast<int>(shadowMapsManger->GetShadowMappingTechnique());
		if (ImGui::Combo("Point Lights Shadow Mapping Technique", &currentTechnique, shadowMappingTechniques, SPT_ARRAY_SIZE(shadowMappingTechniques)))
		{
			shadowMapsManger->SetShadowMappingTechnique(static_cast<rsc::EShadowMappingTechnique>(currentTechnique));
		}
	}

	if (lib::SharedPtr<rsc::ddgi::DDGISceneSubsystem> ddgiSubsystem = scene.GetSceneSubsystem<rsc::ddgi::DDGISceneSubsystem>())
	{
		const char* ddgiDebugModes[rsc::ddgi::EDDGIDebugMode::NUM] = { "None", "Illuminance", "Hit Distance", "Debug Rays"};
		int ddgiDebugMode = ddgiSubsystem->GetDebugMode();
		if (ImGui::Combo("DDGI Probes Visualization", &ddgiDebugMode, ddgiDebugModes, SPT_ARRAY_SIZE(ddgiDebugModes)))
		{
			ddgiSubsystem->SetDebugMode(static_cast<rsc::ddgi::EDDGIDebugMode::Type>(ddgiDebugMode));
		}
	}
}

} // spt::rsc
