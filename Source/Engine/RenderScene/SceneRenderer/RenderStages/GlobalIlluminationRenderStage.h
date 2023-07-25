#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class GlobalIlluminationRenderStage : public RenderStage<GlobalIlluminationRenderStage, ERenderStage::GlobalIllumination>
{
public:

	GlobalIlluminationRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
