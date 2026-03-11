#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "RenderGraphTypes.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{
class ViewRenderingSpec;
} // spt::rsc


namespace spt::rsc::ddgi
{
class DDGISceneDS;
} // spt::rsc::ddgi


namespace spt::rsc::sr_denoiser
{

class Denoiser
{
public:

	struct Params
	{
		explicit Params(const ViewRenderingSpec& inViewSpec)
			: viewSpec(inViewSpec)
		{  }

		const ViewRenderingSpec& viewSpec;

		rg::RGTextureViewHandle historyDepthTexture;
		rg::RGTextureViewHandle currentDepthTexture;

		rg::RGTextureViewHandle roughnessTexture;
		rg::RGTextureViewHandle historyRoughnessTexture;

		rg::RGTextureViewHandle linearDepthTexture;

		rg::RGTextureViewHandle motionTexture;

		rg::RGTextureViewHandle normalsTexture;
		rg::RGTextureViewHandle historyNormalsTexture;

		rg::RGTextureViewHandle specularTexture;
		rg::RGTextureViewHandle diffuseTexture;
		rg::RGTextureViewHandle lightDirection;

		rg::RGTextureViewHandle baseColorMetallicTexture;

		Bool blurVarianceEstimate = true;

		Bool enableStableHistoryBlend = false;

		Uint32 wideRadiusPassesNum = 0;

		Bool resetAccumulation;
	};

	struct Result
	{
		rg::RGTextureViewHandle denoisedSpecular;
		rg::RGTextureViewHandle denoisedDiffuse;
		rg::RGTextureViewHandle varianceEstimation;
	};

	explicit Denoiser(rg::RenderGraphDebugName debugName);

	Result Denoise(rg::RenderGraphBuilder& graphBuilder, const Params& params);

	void InvalidateHistory() { m_hasValidHistory = false; }

	rg::RGTextureViewHandle GetHistorySpecularHitDist(rg::RenderGraphBuilder& graphBuilder) const;

	rg::RGTextureViewHandle GetDiffuseHistoryLength(rg::RenderGraphBuilder& graphBuilder) const;

private:

	void UpdateResources(rg::RenderGraphBuilder& graphBuilder, const Params& params);

	Result DenoiseImpl(rg::RenderGraphBuilder& graphBuilder, const Params& params);

	struct SpatialFilterParams
	{
		rg::RGTextureViewHandle inSpecularY_SH2;
		rg::RGTextureViewHandle inDiffuseY_SH2;
		rg::RGTextureViewHandle inDiffSpecCoCg;

		rg::RGTextureViewHandle outSpecular;
		rg::RGTextureViewHandle outDiffuse;

		rg::RGTextureViewHandle specularHistoryLengthTexture;

		rg::RGTextureViewHandle inVariance;
		rg::RGTextureViewHandle intermediateVariance;
		rg::RGTextureViewHandle outVarianceEstimation;
	};

	void ApplySpatialFilter(rg::RenderGraphBuilder& graphBuilder, const SpatialFilterParams& spatialParams, const Params& params);

	rg::RenderGraphDebugName m_debugName;
	
	lib::SharedPtr<rdr::TextureView> m_historySpecularY_SH2;
	lib::SharedPtr<rdr::TextureView> m_historyDiffuseY_SH2;
	lib::SharedPtr<rdr::TextureView> m_historyDiffSpecCoCg;

	lib::SharedPtr<rdr::TextureView> m_historySpecularHitDist;

	lib::SharedPtr<rdr::TextureView> m_specularHistoryLengthTexture;
	lib::SharedPtr<rdr::TextureView> m_historySpecularHistoryLengthTexture;

	lib::SharedPtr<rdr::TextureView> m_diffuseHistoryLengthTexture;
	lib::SharedPtr<rdr::TextureView> m_historyDiffuseHistoryLengthTexture;

	lib::SharedPtr<rdr::TextureView> m_fastHistorySpecularTexture;
	lib::SharedPtr<rdr::TextureView> m_fastHistorySpecularOutputTexture;

	lib::SharedPtr<rdr::TextureView> m_fastHistoryDiffuseTexture;
	lib::SharedPtr<rdr::TextureView> m_fastHistoryDiffuseOutputTexture;

	lib::SharedPtr<rdr::TextureView> m_temporalVarianceSpecularTexture;
	lib::SharedPtr<rdr::TextureView> m_historyTemporalVarianceSpecularTexture;

	lib::SharedPtr<rdr::TextureView> m_temporalVarianceDiffuseTexture;
	lib::SharedPtr<rdr::TextureView> m_historyTemporalVarianceDiffuseTexture;

	Bool m_hasValidHistory;
};

} // spt::rsc::sr_denoiser
