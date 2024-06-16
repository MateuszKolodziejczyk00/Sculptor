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
#include "ImGui/DockBuilder.h"
#include "ProfilerUIView.h"
#include "UIElements/ApplicationUI.h"
#include "EngineFrame.h"

namespace spt::ed
{

SPT_DEFINE_LOG_CATEGORY(SandboxUI, true);


SandboxUIView::SandboxUIView(const scui::ViewDefinition& definition)
	: Super(definition)
	, m_sceneViewName(CreateUniqueName("Scene"))
	, m_sanboxUIViewName(CreateUniqueName("SandboxUI"))
{
	m_renderViewSettingsName = AddChild(lib::MakeShared<rsc::RenderViewSettingsUIView>(scui::ViewDefinition("Render View Settings"), m_renderer.GetRenderView()));
	m_renderSceneSettingsName = AddChild(lib::MakeShared<rsc::RenderSceneSettingsUIView>(scui::ViewDefinition("Render Scene Settings"), m_renderer.GetRenderScene()));
	m_profilerPanelName = AddChild(lib::MakeShared<prf::ProfilerUIView>(scui::ViewDefinition("ProfilerView")));
}

void SandboxUIView::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID,
			  ui::Split(ui::ESplit::Horizontal, 0.2f,
						ui::DockWindow(m_profilerPanelName),
						ui::Split(ui::ESplit::Horizontal, 0.8f,
								  ui::DockWindow(m_sceneViewName),
								  ui::Split(ui::ESplit::Vertical, 0.33f,
											ui::DockWindow(m_sanboxUIViewName),
											ui::Split(ui::ESplit::Vertical, 0.5f,
													  ui::DockWindow(m_renderViewSettingsName),
													  ui::DockWindow(m_renderSceneSettingsName))))));
}

void SandboxUIView::DrawUI()
{
	Super::DrawUI();
	
	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	
	ImGui::Begin(m_sceneViewName.GetData());

	const math::Vector2f imagePosition = ImGui::GetCurrentWindow()->DC.CursorPos;
	const math::Vector2f offset = math::Vector2f(ImGui::GetMousePos()) - imagePosition;
	m_renderer.SetMousePositionOnViewport((offset + math::Vector2f::Constant(0.5f)).cast<Int32>());
	
	const math::Vector2f sceneContentSize = ui::UIUtils::GetWindowContentSize();

	if (sceneContentSize.x() >= 2.f && sceneContentSize.y() >= 2.f)
	{
		const ui::TextureID texture = PrepareViewportTexture(sceneContentSize.cast<Uint32>());

		engn::FrameContext& frame = scui::ApplicationUI::GetCurrentContext().GetCurrentFrame();
		
		js::Launch(SPT_GENERIC_JOB_NAME,
				   [this, &frame, texture = m_viewportTexture]
				   {
					   m_renderer.Tick(frame.GetDeltaTime());
				   },
				   js::JobDef()
					   .SetPriority(js::EJobPriority::High)
					   .ExecuteBefore(frame.GetStageFinishedEvent(engn::EFrameStage::ProcessViewsUpdate)));

		js::Launch(SPT_GENERIC_JOB_NAME,
				   [this, &frame, texture = m_viewportTexture]
				   {
					   m_renderer.ProcessView(frame, lib::Ref(texture));
				   },
				   js::Prerequisites(frame.GetStageBeginEvent(engn::EFrameStage::ProcessViewsRendering)),
				   js::JobDef()
					   .SetPriority(js::EJobPriority::High)
					   .ExecuteBefore(frame.GetStageFinishedEvent(engn::EFrameStage::ProcessViewsRendering)));

		ImGui::Image(texture, sceneContentSize, ImVec2{}, ImVec2{ 1.f, 1.f });
	}

	ImGui::End();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());

	ImGui::Begin(m_sanboxUIViewName.GetData());

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

	Real32 fov = m_renderer.GetFov();
	if (ImGui::SliderFloat("FOV (Degrees)", &fov, 40.f, 90.f))
	{
		m_renderer.SetFov(fov);
	}
	
	Real32 cameraSpeed = m_renderer.GetCameraSpeed();
	if (ImGui::SliderFloat("Camera Speed", &cameraSpeed, 0.1f, 100.f))
	{
		m_renderer.SetCameraSpeed(cameraSpeed);
	}

	Real32 nearPlane = m_renderer.GetNearPlane();
	if (ImGui::SliderFloat("Near Plane", &nearPlane, 0.01f, 10.f))
	{
		m_renderer.SetNearPlane(nearPlane);
	}

	Real32 farPlane = m_renderer.GetFarPlane();
	if (ImGui::SliderFloat("Far Plane", &farPlane, 5.f, 100.f))
	{
		m_renderer.SetFarPlane(farPlane);
	}

	const ImGuiID id = ImGui::GetID("Capture Render Graph");
	ui::UIContext uiContext = scui::ApplicationUI::GetCurrentContext().GetUIContext();
	ui::UIUtils::SetShortcut(uiContext, id, ui::ShortcutBinding::Create(inp::EKey::LShift, inp::EKey::C));

	ImGui::Checkbox("Auto Sun Movement", &m_renderer.sunMovement);

	if (m_renderer.sunMovement)
	{
		ImGui::SliderFloat("Sun Movement Speed", &m_renderer.sunMovementSpeed, 0.01f, 1.f);
	}
	else
	{
		m_renderer.sunAngleDirty |= ImGui::SliderAngle("Sun Angle Pitch", &m_renderer.sunAnglePitch, -180.f, 180.f);
		m_renderer.sunAngleDirty |= ImGui::SliderAngle("Sun Angle Yaw", &m_renderer.sunAngleYaw, 0.f, 360.f);

	}

	if (ImGui::Button("Capture Render Graph"))
	{
		m_renderer.CreateRenderGraphCapture();
	}
}

void SandboxUIView::DrawJobSystemTestsUI()
{
	ImGui::Text("Job System");;
		
	if (ImGui::Button("Test (Multiple small jobs)"))
	{
		lib::DynamicArray<js::Job> jobs;

		for (SizeType i = 0; i < 50; ++i)
		{
			jobs.emplace_back(js::Launch(SPT_GENERIC_JOB_NAME, []
										 {
											 SPT_PROFILER_SCOPE("Job 1");
											 for (Int64 x = 0; x < 4000; ++x)
											 {
												 SPT_MAYBE_UNUSED
												 float s = std::sin(std::cos(std::sin(std::cos(static_cast<float>(x)))));
												 if (x % 50 == 0)
												 {
													 js::Launch(SPT_GENERIC_JOB_NAME, []
																{
																	SPT_PROFILER_SCOPE("Job 2");
																	for (Int64 x = 0; x < 100; ++x)
																	{
																		SPT_MAYBE_UNUSED
																		float s = std::sin(std::cos(std::sin(std::cos(static_cast<float>(x)))));
																	}
																});
												 }
											 }
										 }, js::JobDef().SetPriority(js::EJobPriority::High)));

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

ui::TextureID SandboxUIView::PrepareViewportTexture(math::Vector2u resolution)
{
	if (!m_viewportTexture || m_viewportTexture->GetResolution2D() != resolution)
	{
		rhi::TextureDefinition textureDef;
		textureDef.resolution = resolution;
		textureDef.format     = scui::ApplicationUI::GetCurrentContext().GetBackBufferFormat();
		textureDef.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest);
		m_viewportTexture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Viewport Texture"), textureDef, rhi::EMemoryUsage::GPUOnly);
	}

	return rdr::UIBackend::GetUITextureID(lib::Ref(m_viewportTexture));
}

} // spt::ed
