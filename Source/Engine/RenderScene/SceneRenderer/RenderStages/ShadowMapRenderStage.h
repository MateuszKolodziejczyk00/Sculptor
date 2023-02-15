#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"
#include "RHICore/RHITextureTypes.h"


namespace spt::rsc
{

class ShadowMapRenderStage : public RenderStage<ShadowMapRenderStage, ERenderStage::ShadowMap>
{
public:

	static rhi::EFragmentFormat GetDepthFormat();

	ShadowMapRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
