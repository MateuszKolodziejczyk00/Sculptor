#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"
#include "Utils/Denoisers/VisbilityDenoiser/VisibilityDataDenoiser.h"
#include "SceneRenderer/RenderStages/Utils/RTShadowMaskRenderer.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

class DirectionalLightShadowMasksRenderStage : public RenderStage<DirectionalLightShadowMasksRenderStage, ERenderStage::DirectionalLightsShadowMasks>
{
protected:

	using Base = RenderStage<DirectionalLightShadowMasksRenderStage, ERenderStage::DirectionalLightsShadowMasks>;

public:

	DirectionalLightShadowMasksRenderStage();

	// Begin RenderStageBase overrides
	void BeginFrame(const RenderScene& renderScene, ViewRenderingSpec& viewSpec);
	// End RenderStageBase overrides

	void OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	RTShadowMaskRenderer m_shadowMaskRenderer;
};

} // spt::rsc
