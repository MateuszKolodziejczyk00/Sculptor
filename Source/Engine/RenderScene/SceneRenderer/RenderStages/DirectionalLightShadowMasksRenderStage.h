#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"
#include "Denoisers/VisbilityDenoiser/VisibilityDataDenoiser.h"
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

	using Super = RenderStage<DirectionalLightShadowMasksRenderStage, ERenderStage::DirectionalLightsShadowMasks>;

public:

	DirectionalLightShadowMasksRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, RenderStageExecutionContext& stageContext);

private:

	void PrepareRenderers(const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	lib::HashMap<RenderSceneEntity, RTShadowMaskRenderer> m_shadowMaskRenderers;
};

} // spt::rsc