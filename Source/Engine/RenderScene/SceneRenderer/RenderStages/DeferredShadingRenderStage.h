#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"


namespace spt::rsc
{

class DeferredShadingRenderStage : public RenderStage<DeferredShadingRenderStage, ERenderStage::DeferredShading>
{
protected:

	using Super = RenderStage<DeferredShadingRenderStage, ERenderStage::DeferredShading>;

public:

	DeferredShadingRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
