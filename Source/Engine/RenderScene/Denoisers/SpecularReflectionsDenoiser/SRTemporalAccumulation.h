#pragma once

#include "SculptorCoreTypes.h"
#include "Denoisers/DenoiserTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc::ddgi
{
class DDGISceneDS;
} // spt::rsc::ddgi


namespace spt::rsc::sr_denoiser
{

struct TemporalAccumulationParameters : public denoising::DenoiserBaseParams
{
	using denoising::DenoiserBaseParams::DenoiserBaseParams;

	explicit TemporalAccumulationParameters(const denoising::DenoiserBaseParams& inParams)
		: denoising::DenoiserBaseParams(inParams)
	{  }

	rg::RGTextureViewHandle specularColorAndRoughnessTexture;
	rg::RGTextureViewHandle accumulatedSamplesNumTexture;
	rg::RGTextureViewHandle historyAccumulatedSamplesNumTexture;
	rg::RGTextureViewHandle temporalVarianceTexture;
	rg::RGTextureViewHandle historyTemporalVarianceTexture;

	rg::RGTextureViewHandle historyNormalsTexture;

	rg::RGTextureViewHandle historyRoughnessTexture;
	rg::RGTextureViewHandle outputRoughnessTexture;

	lib::MTHandle<ddgi::DDGISceneDS> ddgiSceneDS;
};


void ApplyTemporalAccumulation(rg::RenderGraphBuilder& graphBuilder, const TemporalAccumulationParameters& params);

} // spt::rsc::sr_denoiser