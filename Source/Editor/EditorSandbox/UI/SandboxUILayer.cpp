#include "UI/SandboxUILayer.h"
#include "ImGui/SculptorImGui.h"
#include "UIUtils.h"
#include "Renderer/SandboxRenderer.h"
#include "JobSystem.h"
#include "UIElements/UIWindowTypes.h"

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
	DrawJobSystemTestsUI();
	ImGui::Separator();

	ImGui::End();
}

void SandboxUILayer::DrawJobSystemTestsUI()
{
	ImGui::Text("Job System");;
		
	if (ImGui::Button("Test"))
	{
		lib::DynamicArray<js::Job> jobs;

		for (SizeType i = 0; i < 5; ++i)
		{
			jobs.emplace_back(js::Launch([]
										 {
											 for (Int64 x = 0; x < 1000000; ++x)
											 {
												 SPT_MAYBE_UNUSED
												 float s = std::sin(std::cos(std::sin(std::cos(static_cast<float>(x)))));
												 if (x % 50 == 0)
												 {
													 js::Launch([]
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

			js::Launch([i]
					   {
						   SPT_LOG_TRACE(SandboxUI, "Finished {0}", i);
					   }, js::Prerequisites(jobs.back()));
		}
	}
}

} // spt::ed
