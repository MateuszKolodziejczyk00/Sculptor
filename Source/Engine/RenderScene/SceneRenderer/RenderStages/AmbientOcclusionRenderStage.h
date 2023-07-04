#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class AmbientOcclusionRenderStage : public RenderStage<AmbientOcclusionRenderStage, ERenderStage::AmbientOcclusion>
{
public:

	AmbientOcclusionRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
