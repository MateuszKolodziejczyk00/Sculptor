#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class PostProcessPreAARenderStage : public RenderStage<PostProcessPreAARenderStage, ERenderStage::PostProcessPreAA>
{
public:

	PostProcessPreAARenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc