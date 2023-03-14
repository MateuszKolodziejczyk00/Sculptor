
#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"


namespace spt::rsc
{

class MotionAndDepthRenderStage : public RenderStage<MotionAndDepthRenderStage, ERenderStage::MotionAndDepth>
{
public:

	static rhi::EFragmentFormat GetMotionFormat();

	MotionAndDepthRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
