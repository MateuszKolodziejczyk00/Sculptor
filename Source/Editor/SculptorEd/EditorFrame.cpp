#include "EditorFrame.h"
#include "Renderer.h"
#include "ImGui/SculptorImGui.h"


namespace spt::ed
{

EditorFrameContext::EditorFrameContext()
{ }

void EditorFrameContext::DoStagesTransition(engn::EFrameStage::Type prevStage, engn::EFrameStage::Type nextStage)
{
	Super::DoStagesTransition(prevStage, nextStage);

	if (nextStage == engn::EFrameStage::RenderingBegin)
	{
		rdr::Renderer::BeginFrame(GetFrameIdx());
	}

	if (prevStage == engn::EFrameStage::UpdatingEnd)
	{
		// ImGui calls submits commands to queues here which is not threadsafe so we have to make sure that no other rendering is happening
		// Currently this is the only place where this is guaranteed
		ImGuiIO& imGuiIO = ImGui::GetIO();
		if (imGuiIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			// Disable warnings, so that they are not spamming when ImGui backend allocates memory not from pools
			RENDERER_DISABLE_VALIDATION_WARNINGS_SCOPE
			ImGui::RenderPlatformWindowsDefault();
		}
	}
}

} // spt::ed
