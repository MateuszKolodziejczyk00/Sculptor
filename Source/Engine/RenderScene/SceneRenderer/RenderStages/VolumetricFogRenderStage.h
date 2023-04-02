#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class VolumetricFogRenderStage : public RenderStage<VolumetricFogRenderStage, ERenderStage::VolumetricFog>
{
public:

	VolumetricFogRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
