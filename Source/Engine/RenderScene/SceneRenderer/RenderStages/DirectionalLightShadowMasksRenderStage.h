#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"
#include "Denoisers/VisbilityDenoiser/VisibilityDataDenoiser.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

struct DirectionalLightRTShadowsData
{
	DirectionalLightRTShadowsData()
		: denoiser(RG_DEBUG_NAME("Directional Light Denoiser"))
	{ }

	visibility_denoiser::Denoiser denoiser;
};


struct ViewShadowMasksDataComponent
{
	lib::HashMap<RenderSceneEntity, DirectionalLightRTShadowsData> directionalLightsShadowData;
};


class DirectionalLightShadowMasksRenderStage : public RenderStage<DirectionalLightShadowMasksRenderStage, ERenderStage::DirectionalLightsShadowMasks>
{
protected:

	using Super = RenderStage<DirectionalLightShadowMasksRenderStage, ERenderStage::DirectionalLightsShadowMasks>;

public:

	DirectionalLightShadowMasksRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, RenderStageExecutionContext& stageContext);
};

} // spt::rsc