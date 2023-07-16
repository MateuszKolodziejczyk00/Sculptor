#include "UI/SandboxUIView.h"
#include "ImGui/SculptorImGui.h"
#include "UIUtils.h"
#include "Renderer/SandboxRenderer.h"
#include "JobSystem.h"
#include "Types/Window.h"
#include "Renderer.h"
#include "Shaders/ShaderTypes.h"
#include "RenderView/RenderViewSettingsUIView.h"
#include "RenderScene/RenderSceneSettingsUIView.h"
#include "DDGI/DDGISceneSubsystem.h"
#include "Types/UIBackend.h"
#include "Types/Texture.h"
#include "DDGI/DDGITypes.h"

namespace spt::ed
{

SPT_DEFINE_LOG_CATEGORY(SandboxUI, true);

SandboxUIView::SandboxUIView(const scui::ViewDefinition& definition, SandboxRenderer& renderer)
	: Super(definition)
	, m_renderer(&renderer)
{
	AddChild(lib::MakeShared<rsc::RenderViewSettingsUIView>(scui::ViewDefinition("Render View Settings"), renderer.GetRenderView()));
	AddChild(lib::MakeShared<rsc::RenderSceneSettingsUIView>(scui::ViewDefinition("Render Scene Settings"), renderer.GetRenderScene()));
}

void SandboxUIView::DrawUI()
{
	Super::DrawUI();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	
	ImGui::Begin("Scene");

	const ui::TextureID sceneTexture = m_renderer->GetUITextureID();
	if (sceneTexture)
	{
		const math::Vector2f sceneContentSize = ui::UIUtils::GetWindowContentSize();

		m_renderer->SetImageSize(sceneContentSize.cast<Uint32>());

		const math::Vector2u fullTextureResolution = m_renderer->GetDisplayTextureResolution();
		const math::Vector2f imageMaxUV = sceneContentSize.array() / fullTextureResolution.cast<Real32>().array();
		ImGui::Image(sceneTexture, sceneContentSize, ImVec2{}, ImVec2{ imageMaxUV.x(), imageMaxUV.y() });
	}

	ImGui::End();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());

	ImGui::Begin("SandboxUI");

	ImGui::Separator();
	DrawRendererSettings();
	ImGui::Separator();
	DrawJobSystemTestsUI();
	ImGui::Separator();
	
	ImGui::Text("Shaders");
#if WITH_SHADERS_HOT_RELOAD
	if (ImGui::Button("HotReload Shaders"))
	{
		rdr::Renderer::HotReloadShaders();
	}
#endif WITH_SHADERS_HOT_RELOAD
	ImGui::End();
}

void SandboxUIView::DrawRendererSettings()
{
	ImGui::Text("Renderer Settings");

	const lib::SharedPtr<rdr::Window>& window = m_renderer->GetWindow();

	Bool vsyncEnabled = window->GetRHI().IsVSyncEnabled();
	if (ImGui::Checkbox("Enable VSync", &vsyncEnabled))
	{
		window->GetRHI().SetVSyncEnabled(vsyncEnabled);
	}

	Real32 fov = m_renderer->GetFov();
	if (ImGui::SliderFloat("FOV (Degrees)", &fov, 40.f, 90.f))
	{
		m_renderer->SetFov(fov);
	}
	
	Real32 cameraSpeed = m_renderer->GetCameraSpeed();
	if (ImGui::SliderFloat("Camera Speed", &cameraSpeed, 0.1f, 100.f))
	{
		m_renderer->SetCameraSpeed(cameraSpeed);
	}

	Real32 nearPlane = m_renderer->GetNearPlane();
	if (ImGui::SliderFloat("Near Plane", &nearPlane, 0.01f, 10.f))
	{
		m_renderer->SetNearPlane(nearPlane);
	}

	Real32 farPlane = m_renderer->GetFarPlane();
	if (ImGui::SliderFloat("Far Plane", &farPlane, 5.f, 100.f))
	{
		m_renderer->SetFarPlane(farPlane);
	}

}

void SandboxUIView::DrawJobSystemTestsUI()
{
	ImGui::Text("Job System");;
		
	if (ImGui::Button("Test (Multiple small jobs)"))
	{
		lib::DynamicArray<js::Job> jobs;

		for (SizeType i = 0; i < 5; ++i)
		{
			jobs.emplace_back(js::Launch(SPT_GENERIC_JOB_NAME, []
										 {
											 for (Int64 x = 0; x < 1000000; ++x)
											 {
												 SPT_MAYBE_UNUSED
												 float s = std::sin(std::cos(std::sin(std::cos(static_cast<float>(x)))));
												 if (x % 50 == 0)
												 {
													 js::Launch(SPT_GENERIC_JOB_NAME, []
																{
																	for (Int64 x = 0; x < 100; ++x)
																	{
																		SPT_MAYBE_UNUSED
																		float s = std::sin(std::cos(std::sin(std::cos(static_cast<float>(x)))));
																	}
																});
												 }
											 }
										 }, js::EJobPriority::High));

			js::Launch(SPT_GENERIC_JOB_NAME, [i]
					   {
						   SPT_LOG_TRACE(SandboxUI, "Finished {0}", i);
					   }, js::Prerequisites(jobs.back()));
		}
	}

	if (ImGui::Button("Test (Then)"))
	{
		js::Launch(SPT_GENERIC_JOB_NAME, []
				   {
					   SPT_LOG_TRACE(SandboxUI, "1");
				   }).Then(SPT_GENERIC_JOB_NAME, []
				   {
					   SPT_LOG_TRACE(SandboxUI, "2");
				   }).Then(SPT_GENERIC_JOB_NAME, []
				   {
					   SPT_LOG_TRACE(SandboxUI, "3");
				   }).Then(SPT_GENERIC_JOB_NAME, []
				   {
					   SPT_LOG_TRACE(SandboxUI, "4");
				   }).Then(SPT_GENERIC_JOB_NAME, []
				   {
					   SPT_LOG_TRACE(SandboxUI, "5");
				   });
	}
}

} // spt::ed
