#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"


namespace spt::rsc
{

class SpecularReflectionsRenderStage : public RenderStage<SpecularReflectionsRenderStage, ERenderStage::SpecularReflections>
{
public:

	SpecularReflectionsRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
