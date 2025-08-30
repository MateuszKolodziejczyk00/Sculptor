#include "RenderViewSettingsUIView.h"
#include "View/RenderView.h"
#include "ImGui/SculptorImGui.h"
#include "Camera/CameraSettings.h"
#include "ImGui/DockBuilder.h"
#include "View/Systems/TemporalAAViewRenderSystem.h"
#include "Techniques/TemporalAA//StandardTAARenderer.h"
#include "Techniques/TemporalAA//DLSSRenderer.h"
#include "Techniques/TemporalAA/TemporalAccumulationRenderer.h"

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
	
	if (CameraLensSettingsComponent* lensSettings = view.GetBlackboard().Find<CameraLensSettingsComponent>())
	{
		if (ImGui::CollapsingHeader("Lens Settings"))
		{
			ImGui::ColorEdit3("Lens Dirt Intensity", lensSettings->lensDirtIntensity.data());
			ImGui::ColorEdit3("Lens Dirt Threshold", lensSettings->lensDirtThreshold.data());
		}
	}

	if (lib::SharedPtr<rsc::TemporalAAViewRenderSystem> taaSystem = view.FindRenderSystem<rsc::TemporalAAViewRenderSystem>())
	{
		const char* modes[] = { "None", "DLSS", "Standard TAA", "Temporal Accumulation"};
		const lib::StringView currentMode = taaSystem->GetRendererName();
		const auto foundMode = std::find(std::begin(modes), std::end(modes), currentMode);
		SPT_CHECK(foundMode != std::end(modes));
		int currentModeIdx = static_cast<int>(std::distance(std::begin(modes), foundMode));

		if (ImGui::Combo("Anti Aliasing Mode", &currentModeIdx, modes, SPT_ARRAY_SIZE(modes)))
		{
			lib::UniquePtr<gfx::TemporalAARenderer> newRenderer;

			if (currentModeIdx == 1)
			{
				newRenderer = std::make_unique<gfx::DLSSRenderer>();
			}
			else if (currentModeIdx == 2)
			{
				newRenderer = std::make_unique<gfx::StandardTAARenderer>();
			}
			else if (currentModeIdx == 3)
			{
				newRenderer = std::make_unique<gfx::TemporalAccumulationRenderer>();
			}

			if (newRenderer && !newRenderer->Initialize(gfx::TemporalAAInitSettings{}))
			{
				newRenderer.reset();
			}

			taaSystem->SetTemporalAARenderer(std::move(newRenderer));
		}
	}
}

} // spt::rsc
