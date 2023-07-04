#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class GenerateGeometryNormalsRenderStage : public RenderStage<GenerateGeometryNormalsRenderStage, ERenderStage::GenerateGeometryNormals>
{
public:

	GenerateGeometryNormalsRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
