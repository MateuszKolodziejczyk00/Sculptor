#include "RenderViewSettingsUIView.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "View/RenderView.h"
#include "ImGui/DockBuilder.h"
#include "SceneRenderer/SceneRenderer.h"
#include "Engine.h"

namespace spt::rsc
{

RenderViewSettingsUIView::RenderViewSettingsUIView(const scui::ViewDefinition& definition, RenderView& renderView)
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

		DrawUIForView(m_renderView);
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

	const SceneRendererDLLModuleAPI* sceneRendererAPI = engn::Engine::Get().GetModulesManager().GetModuleAPI<SceneRendererDLLModuleAPI>();

	rsc::ShadingRenderViewSettings shadingSettings = sceneRendererAPI->GetShadingViewSettings(view);

	Bool settingsChanged = false;

	const char* modes[] = { "None", "DLSS", "Standard TAA", "Temporal Accumulation"};
	if (ImGui::Combo("Anti Aliasing Mode", reinterpret_cast<int*>(&shadingSettings.upscalingMethod), modes, SPT_ARRAY_SIZE(modes)))
	{
		settingsChanged = true;
	}

	if (settingsChanged)
	{
		sceneRendererAPI->SetShadingViewSettings(view, shadingSettings);
	}
}

} // spt::rsc
