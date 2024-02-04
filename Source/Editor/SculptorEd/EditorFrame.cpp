#include "EditorFrame.h"
#include "Renderer.h"


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
}

} // spt::ed
