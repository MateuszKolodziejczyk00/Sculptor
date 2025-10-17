#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"


namespace spt::rsc
{

class TransparencyRenderStage : public RenderStage<TransparencyRenderStage, ERenderStage::Transparency>
{
protected:

	using Super = RenderStage<TransparencyRenderStage, ERenderStage::Transparency>;

public:

	TransparencyRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:
};

} // spt::rsc
