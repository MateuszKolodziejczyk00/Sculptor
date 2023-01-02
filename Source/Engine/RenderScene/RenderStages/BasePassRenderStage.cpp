#include "RenderStages/BasePassRenderStage.h"


namespace spt::rsc
{

BasePassRenderStage::BasePassRenderStage()
{ }

void BasePassRenderStage::Render(rg::RenderGraphBuilder& graphBuilder, const BasePassStageInput&& input)
{
	SPT_PROFILER_FUNCTION();

	m_preRenderBasePassDelegate.Broadcast(graphBuilder);

	SPT_CHECK_NO_ENTRY_MSG("TODO");
	//graphBuilder.AddRenderPass()

	m_renderBasePassDelegate.Broadcast(graphBuilder);
}

PreRenderBasePassDelegate& BasePassRenderStage::GetPreRenderBasePassDelegate()
{
	return m_preRenderBasePassDelegate;
}

RenderBasePassDelegate& BasePassRenderStage::GetRenderBasePassDelegate()
{
	return m_renderBasePassDelegate;
}

} // spt::rsc
