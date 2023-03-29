#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"
#include "RHICore/RHITextureTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rsc
{

class ShadowMapRenderStage : public RenderStage<ShadowMapRenderStage, ERenderStage::ShadowMap>
{
public:

	static rhi::EFragmentFormat GetRenderedDepthFormat();

	ShadowMapRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	void RenderDepth(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext, const rg::RGTextureViewHandle depthRenderTargetView);

	void RenderDPCF(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext, const rg::RGTextureHandle shadowMap);
	void RenderMSM(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext, const rg::RGTextureHandle shadowMap);
};

} // spt::rsc
