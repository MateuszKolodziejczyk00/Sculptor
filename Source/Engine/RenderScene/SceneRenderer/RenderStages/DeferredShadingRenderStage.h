#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"
#include "Utils/StochasticDIRenderer.h"


namespace spt::rsc
{

class DeferredShadingRenderStage : public RenderStage<DeferredShadingRenderStage, ERenderStage::DeferredShading>
{
protected:

	using Super = RenderStage<DeferredShadingRenderStage, ERenderStage::DeferredShading>;

public:

	DeferredShadingRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

protected:

	stochastic_di::Renderer m_stochasticDIRenderer;
};

} // spt::rsc
