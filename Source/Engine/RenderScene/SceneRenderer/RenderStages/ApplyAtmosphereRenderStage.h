#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class ApplyAtmosphereRenderStage : public RenderStage<ApplyAtmosphereRenderStage, ERenderStage::ApplyAtmosphere>
{
public:

	ApplyAtmosphereRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
