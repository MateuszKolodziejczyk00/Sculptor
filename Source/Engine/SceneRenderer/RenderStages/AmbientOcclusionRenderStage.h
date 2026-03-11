#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStages/RenderStage.h"
#include "Utils/Denoisers/VisbilityDenoiser/VisibilityDataDenoiser.h"

namespace spt::rsc
{

class AmbientOcclusionRenderStage : public RenderStage<AmbientOcclusionRenderStage, ERenderStage::AmbientOcclusion>
{
protected:

	using Super = RenderStage<AmbientOcclusionRenderStage, ERenderStage::AmbientOcclusion>;

public:

	AmbientOcclusionRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	visibility_denoiser::Denoiser m_denoiser;
};

} // spt::rsc
