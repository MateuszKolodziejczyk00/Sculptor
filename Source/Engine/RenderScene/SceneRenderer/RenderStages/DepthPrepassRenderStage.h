#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"


namespace spt::rsc
{

class DepthPrepassRenderStage : public RenderStage<DepthPrepassRenderStage, ERenderStage::DepthPrepass>
{
public:

	static rhi::EFragmentFormat GetVelocityFormat();
	static rhi::EFragmentFormat GetDepthFormat();

	DepthPrepassRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
