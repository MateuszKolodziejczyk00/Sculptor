#pragma once

#include "SculptorCoreTypes.h"
#include "Denoisers/DenoiserTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{
class SharcCacheDS;
} // spt::rsc


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

	rg::RGTextureViewHandle currentRoughnessTexture;
	rg::RGTextureViewHandle historyRoughnessTexture;

	rg::RGTextureViewHandle historyNormalsTexture;

	rg::RGTextureViewHandle specularHistoryLengthTexture;
	rg::RGTextureViewHandle historySpecularHistoryLengthTexture;
	rg::RGTextureViewHandle diffuseHistoryLengthTexture;
	rg::RGTextureViewHandle historyDiffuseHistoryLengthTexture;

	rg::RGTextureViewHandle currentSpecularTexture;
	rg::RGTextureViewHandle currentDiffuseTexture;

	rg::RGTextureViewHandle historySpecularTexture;
	rg::RGTextureViewHandle historyDiffuseTexture;

	rg::RGTextureViewHandle temporalVarianceSpecularTexture;
	rg::RGTextureViewHandle historyTemporalVarianceSpecularTexture;
	rg::RGTextureViewHandle temporalVarianceDiffuseTexture;
	rg::RGTextureViewHandle historyTemporalVarianceDiffuseTexture;

	rg::RGTextureViewHandle fastHistorySpecularTexture;
	rg::RGTextureViewHandle fastHistorySpecularOutputTexture;
	rg::RGTextureViewHandle fastHistoryDiffuseTexture;
	rg::RGTextureViewHandle fastHistoryDiffuseOutputTexture;

	rg::RGTextureViewHandle baseColorMetallic;

	lib::MTHandle<SharcCacheDS> sharcCacheDS;

	Bool enableStableHistoryBlend            = true;
	Bool enableDisocclusionFixFromLightCache = true;
};

void ApplyTemporalAccumulation(rg::RenderGraphBuilder& graphBuilder, const TemporalAccumulationParameters& params);

} // spt::rsc::sr_denoiser