#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"


namespace spt::rsc
{

struct DepthPrepassMetaData
{
	Bool allowJittering = true;
};


class DepthPrepassRenderStage : public RenderStage<DepthPrepassRenderStage, ERenderStage::DepthPrepass>
{
public:

	static rhi::EFragmentFormat GetDepthFormat();

	DepthPrepassRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	void ExecuteDepthPrepass(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext, rg::RGTextureViewHandle depthTarget, const DepthPrepassMetaData& metaData);
};

} // spt::rsc
