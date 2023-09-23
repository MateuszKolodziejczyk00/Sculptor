#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class DownsampleGeometryTexturesRenderStage : public RenderStage<DownsampleGeometryTexturesRenderStage, ERenderStage::DownsampleGeometryTextures>
{
public:

	DownsampleGeometryTexturesRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
