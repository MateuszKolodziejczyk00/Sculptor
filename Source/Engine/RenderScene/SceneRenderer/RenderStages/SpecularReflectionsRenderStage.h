#pragma once

#include "SculptorCoreTypes.h"
#include "RenderStage.h"
#include "SceneRenderer/RenderStages/Utils/SRSpatiotemporalResampler.h"
#include "Denoisers/SpecularReflectionsDenoiser/SRDenoiser.h"
#include "SceneRenderer/RenderStages/Utils/VariableRateTexture.h"
#include "Utils/SharcGICache.h"


namespace spt::rsc
{

class SpecularReflectionsRenderStage : public RenderStage<SpecularReflectionsRenderStage, ERenderStage::SpecularReflections>
{
protected:

	using Base = RenderStage<SpecularReflectionsRenderStage, ERenderStage::SpecularReflections>;

public:

	SpecularReflectionsRenderStage();

	// Begin RenderStageBase overrides
	virtual void BeginFrame(const RenderScene& renderScene, ViewRenderingSpec& viewSpec) override;
	// End RenderStageBase overrides

	void OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

private:

	void RenderVariableRateTexture(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext);

	void UpdateSharcCache(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	sr_restir::SpatiotemporalResampler m_resampler;
	sr_denoiser::Denoiser m_denoiser;

	vrt::VariableRateRenderer m_variableRateRenderer;

	lib::UniquePtr<SharcGICache> m_sharcCache;

	Bool m_renderHalfResReflections = false;
};

} // spt::rsc
