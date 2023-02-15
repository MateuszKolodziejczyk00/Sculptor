#include "ShadowMapRenderStage.h"

namespace spt::rsc
{

rhi::EFragmentFormat ShadowMapRenderStage::GetDepthFormat()
{
	return rhi::EFragmentFormat::D16_UN_Float;
}

ShadowMapRenderStage::ShadowMapRenderStage()
{ }

void ShadowMapRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();


}

} // spt::rsc
