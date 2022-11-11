#include "UI/SandboxUILayer.h"
#include "ImGui/SculptorImGui.h"
#include "UIUtils.h"
#include "Renderer/SandboxRenderer.h"
#include "JobSystem.h"
#include "UIElements/UIWindowTypes.h"
#include "Types/Window.h"

namespace spt::ed
{

SPT_DEFINE_LOG_CATEGORY(SandboxUI, true);

SandboxUILayer::SandboxUILayer(const scui::LayerDefinition& definition, const SandboxRenderer& renderer)
	: Super(definition)
	, m_renderer(&renderer)
{ }

void SandboxUILayer::DrawUI()
{
	Super::DrawUI();

	ImGuiWindowClass windowsClass;
	windowsClass.ClassId = scui::CurrentWindowBuildingContext::GetCurrentWindowDockspaceID();

	ImGui::SetNextWindowClass(&windowsClass);

	ImGui::Begin("RendererTexture");
	ImGui::Image(m_renderer->GetUITextureID(), ui::UIUtils::GetWindowContentSize());
	ImGui::End();

	ImGui::SetNextWindowClass(&windowsClass);

	ImGui::Begin("SandboxUI");
	
	ImGui::Separator();
	DrawRendererSettings();
	ImGui::Separator();
	DrawJobSystemTestsUI();
	ImGui::Separator();

	ImGui::End();
}

void SandboxUILayer::DrawRendererSettings()
{
	ImGui::Text("Renderer Settings");

	const lib::SharedPtr<rdr::Window>& window = m_renderer->GetWindow();

	Bool vsyncEnabled = window->GetRHI().IsVSyncEnabled();
	if (ImGui::Checkbox("Enable VSync", &vsyncEnabled))
	{
		window->GetRHI().SetVSyncEnabled(vsyncEnabled);
	}
}

void SandboxUILayer::DrawJobSystemTestsUI()
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
