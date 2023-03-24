#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class AntiAliasingRenderStage : public RenderStage<AntiAliasingRenderStage, ERenderStage::AntiAliasing>
{
public:

	AntiAliasingRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc
