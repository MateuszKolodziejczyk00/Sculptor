#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"
#include "Techniques/TemporalAA/TemporalAATypes.h"


namespace spt::rsc
{

class PreRenderingRenderStage : public RenderStage<PreRenderingRenderStage, ERenderStage::PreRendering>
{
protected:

	using Super = RenderStage<PreRenderingRenderStage, ERenderStage::PreRendering>;

public:

	PreRenderingRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
