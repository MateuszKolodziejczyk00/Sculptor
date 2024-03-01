#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class ForwardOpaqueRenderStage : public RenderStage<ForwardOpaqueRenderStage, ERenderStage::ForwardOpaque>
{
protected:

	using Super = RenderStage<ForwardOpaqueRenderStage, ERenderStage::ForwardOpaque>;

public:

	static RenderTargetFormatsDef GetRenderTargetFormats(const SceneRendererSettings& sceneRendererSettings);

	ForwardOpaqueRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);
};

} // spt::rsc