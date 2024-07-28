#pragma once

#include "SculptorCoreTypes.h"
#include "Denoisers/VisbilityDenoiser/VisibilityDataDenoiser.h"
#include "RenderSceneRegistry.h"
#include "SceneRenderer/RenderStages/Utils/VariableRateTexture.h"


namespace spt::rsc
{

class RenderScene;
class ViewRenderingSpec;


class RTShadowMaskRenderer
{
public:

	RTShadowMaskRenderer();

	void Initialize(RenderSceneEntity entity);

	rg::RGTextureViewHandle Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

private:

	RenderSceneEntity m_lightEntity;

	vrt::VariableRateRenderer m_variableRateRenderer;
	visibility_denoiser::Denoiser m_denoiser;
};

} // spt::rsc