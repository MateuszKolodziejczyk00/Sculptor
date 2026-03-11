#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"

namespace spt::rsc
{

class HDRResolveRenderStage : public RenderStage<HDRResolveRenderStage, ERenderStage::HDRResolve>
{
protected:

	using Super = RenderStage<HDRResolveRenderStage, ERenderStage::HDRResolve>;

public:

	HDRResolveRenderStage();

	// Begin RenderStageBase overrides
	void Initialize(RenderView& renderView);
	// End RenderStageBase overrides

	void OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	lib::SharedPtr<rdr::TextureView> m_tonemappingLUT;
};

} // spt::rsc
