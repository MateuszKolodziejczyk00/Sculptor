#include "RenderViewSettingsUIView.h"
#include "View/RenderView.h"
#include "ImGui/SculptorImGui.h"

namespace spt::rsc
{

RenderViewSettingsUIView::RenderViewSettingsUIView(const scui::ViewDefinition& definition, const lib::SharedPtr<RenderView>& renderView)
	: Super(definition)
	, m_renderView(renderView)
{ }

void RenderViewSettingsUIView::DrawUI()
{
	SPT_PROFILER_FUNCTION();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin("Render View");

	Super::DrawUI();

	const lib::SharedPtr<RenderView> renderView = m_renderView.lock();
	if (renderView)
	{
		DrawUIForView(*renderView);
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
	int aaMode = view.GetAnitAliasingMode();
	if (ImGui::Combo("Anti Aliasing Mode", &aaMode, aaModes, SPT_ARRAY_SIZE(aaModes)))
	{
		view.SetAntiAliasingMode(static_cast<rsc::EAntiAliasingMode::Type>(aaMode));
	}

	const char* debugFeatures[rsc::EDebugFeature::NUM] = { "None", "Show Meshlets", "Show Indirect Lighting"};
	int selectedDebugFeature = view.GetDebugFeature();
	if (ImGui::Combo("Debug Feature", &selectedDebugFeature, debugFeatures, SPT_ARRAY_SIZE(debugFeatures)))
	{
		view.SetDebugFeature(static_cast<rsc::EDebugFeature::Type>(selectedDebugFeature));
	}
}

} // spt::rsc
