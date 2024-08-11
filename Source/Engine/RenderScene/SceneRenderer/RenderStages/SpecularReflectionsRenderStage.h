#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"
#include "SceneRenderer/RenderStages/Utils/SRSpatiotemporalResampler.h"
#include "Denoisers/SpecularReflectionsDenoiser/SRDenoiser.h"
#include "SceneRenderer/RenderStages/Utils/VariableRateTexture.h"


namespace spt::rsc
{

class SpecularReflectionsRenderStage : public RenderStage<SpecularReflectionsRenderStage, ERenderStage::SpecularReflections>
{
public:

	SpecularReflectionsRenderStage();

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	void RenderVariableRateTexture(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

	sr_restir::SpatiotemporalResampler m_resampler;
	sr_denoiser::Denoiser              m_denoiser;

	vrt::VariableRateRenderer m_variableRateRenderer;
};

} // spt::rsc
