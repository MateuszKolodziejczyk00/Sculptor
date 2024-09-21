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
class RenderView;
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
		explicit Params(const RenderView& inRenderView)
			: renderView(inRenderView)
		{  }

		const RenderView& renderView;

		rg::RGTextureViewHandle historyDepthTexture;
		rg::RGTextureViewHandle currentDepthTexture;

		rg::RGTextureViewHandle roughnessTexture;
		rg::RGTextureViewHandle historyRoughnessTexture;

		rg::RGTextureViewHandle linearDepthTexture;

		rg::RGTextureViewHandle motionTexture;

		rg::RGTextureViewHandle normalsTexture;
		rg::RGTextureViewHandle historyNormalsTexture;

		Bool detailPreservingSpatialFilter = false;
	};

	struct Result
	{
		rg::RGTextureViewHandle denoisedTexture;
		rg::RGTextureViewHandle temporalMomentsTexture;
		rg::RGTextureViewHandle spatialStdDevTexture;
		rg::RGTextureViewHandle geometryCoherence;
	};

	explicit Denoiser(rg::RenderGraphDebugName debugName);

	Result Denoise(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params);

private:

	void UpdateResources(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params);

	Result DenoiseImpl(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params);

	struct SpatialFilterParams
	{
		rg::RGTextureViewHandle input;
		rg::RGTextureViewHandle output;
		rg::RGTextureViewHandle stdDev;
		rg::RGTextureViewHandle historySamplesNum;
		rg::RGTextureViewHandle reprojectionConfidence;
		rg::RGTextureViewHandle geometryCoherence;
	};

	void ApplySpatialFilter(rg::RenderGraphBuilder& graphBuilder, const SpatialFilterParams& spatialParams, const Params& params);

	rg::RenderGraphDebugName m_debugName;

	lib::SharedPtr<rdr::TextureView> m_historyTexture;
	
	lib::SharedPtr<rdr::TextureView> m_fastHistoryTexture;
	lib::SharedPtr<rdr::TextureView> m_fastHistoryOutputTexture;
	
	lib::SharedPtr<rdr::TextureView> m_accumulatedSamplesNumTexture;
	lib::SharedPtr<rdr::TextureView> m_historyAccumulatedSamplesNumTexture;
	
	lib::SharedPtr<rdr::TextureView> m_temporalVarianceTexture;
	lib::SharedPtr<rdr::TextureView> m_historyTemporalVarianceTexture;

	Bool m_hasValidHistory;
};

} // spt::rsc::sr_denoiser