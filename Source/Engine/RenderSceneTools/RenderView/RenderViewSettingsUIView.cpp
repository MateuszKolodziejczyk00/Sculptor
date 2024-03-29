#include "RenderViewSettingsUIView.h"
#include "View/RenderView.h"
#include "ImGui/SculptorImGui.h"
#include "Camera/CameraSettings.h"
#include "ImGui/DockBuilder.h"

namespace spt::rsc
{

RenderViewSettingsUIView::RenderViewSettingsUIView(const scui::ViewDefinition& definition, const lib::SharedPtr<RenderView>& renderView)
	: Super(definition)
	, m_renderView(renderView)
	, m_renderViewSettingsName(CreateUniqueName("Render View"))
{ }

void RenderViewSettingsUIView::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID, ui::DockWindow(m_renderViewSettingsName));
}

void RenderViewSettingsUIView::DrawUI()
{
	SPT_PROFILER_FUNCTION();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	if (ImGui::Begin(m_renderViewSettingsName.GetData()))
	{
		Super::DrawUI();

		const lib::SharedPtr<RenderView> renderView = m_renderView.lock();
		if (renderView)
		{
			DrawUIForView(*renderView);
		}
	}

	ImGui::End();
}

void RenderViewSettingsUIView::DrawUIForView(RenderView& view)
{
	math::Vector3f location = view.GetLocation();
	if (ImGui::DragFloat3("Location", &location.x(), 0.1f))
	{
		view.SetLocation(location);
	}
	
	const char* aaModes[rsc::EAntiAliasingMode::NUM] = { "None", "TAA" };
	int aaMode = view.GetAntiAliasingMode();
	if (ImGui::Combo("Anti Aliasing Mode", &aaMode, aaModes, SPT_ARRAY_SIZE(aaModes)))
	{
		view.SetAntiAliasingMode(static_cast<rsc::EAntiAliasingMode::Type>(aaMode));
	}
	
	const RenderSceneEntityHandle viewEntity = view.GetViewEntity();
	if (CameraLensSettingsComponent* lensSettings = viewEntity.try_get<CameraLensSettingsComponent>())
	{
		if (ImGui::CollapsingHeader("Lens Settings"))
		{
			ImGui::ColorEdit3("Lens Dirt Intensity", lensSettings->lensDirtIntensity.data());
			ImGui::ColorEdit3("Lens Dirt Threshold", lensSettings->lensDirtThreshold.data());
		}
	}

	const char* debugFeatures[rsc::EDebugFeature::NUM] = { "None", "Show Meshlets", "Show Indirect Lighting", "Ambient Occlusion"};
	int selectedDebugFeature = view.GetDebugFeature();
	if (ImGui::Combo("Debug Feature", &selectedDebugFeature, debugFeatures, SPT_ARRAY_SIZE(debugFeatures)))
	{
		view.SetDebugFeature(static_cast<rsc::EDebugFeature::Type>(selectedDebugFeature));
	}
}

} // spt::rsc
