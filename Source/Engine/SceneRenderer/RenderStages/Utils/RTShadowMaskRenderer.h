#pragma once

#include "SculptorCoreTypes.h"
#include "Utils/Denoisers/VisbilityDenoiser/VisibilityDataDenoiser.h"
#include "RenderSceneRegistry.h"
#include "RenderStages/Utils/VariableRateTexture.h"


namespace spt::rsc
{

class RenderScene;
class ViewRenderingSpec;
struct SceneRendererInterface;


class RTShadowMaskRenderer
{
public:

	RTShadowMaskRenderer();

	void Initialize(RenderSceneEntity entity);

	void BeginFrame(const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	rg::RGTextureViewHandle Render(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

private:

	RenderSceneEntity m_lightEntity;

	vrt::VariableRateRenderer m_variableRateRenderer;
	visibility_denoiser::Denoiser m_denoiser;

	Bool m_renderHalfRes = false;
};

} // spt::rsc
