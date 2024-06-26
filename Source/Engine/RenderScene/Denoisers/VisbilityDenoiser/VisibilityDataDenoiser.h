#pragma once

#include "SculptorCoreTypes.h"
#include "RenderSceneMacros.h"
#include "Denoisers/DenoiserTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

class RenderView;


namespace visibility_denoiser
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

		rg::RGTextureViewHandle motionTexture;

		rg::RGTextureViewHandle normalsTexture;

		Real32 currentFrameDefaultWeight = 0.16f;

		Real32 accumulatedFramesMaxCount = 12.f;

		Bool useTemporalFilter = true;
	};

	explicit Denoiser(rg::RenderGraphDebugName debugName);

	void Denoise(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params);

private:

	void UpdateResources(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params);

	void DenoiseImpl(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params);

	void ApplySpatialFilters(rg::RenderGraphBuilder& graphBuilder, const denoising::DenoiserBaseParams& params, rg::RGTextureViewHandle denoisedTexture, rg::RGTextureViewHandle historyTexture, rg::RGTextureViewHandle varianceTexture);

	rg::RenderGraphDebugName m_debugName;

	lib::SharedPtr<rdr::TextureView> m_historyTexture;

	lib::SharedPtr<rdr::TextureView> m_accumulatedSamplesNum;
	lib::SharedPtr<rdr::TextureView> m_accumulatedSamplesNumHistory;
	
	lib::SharedPtr<rdr::TextureView> m_temporalMoments;
	lib::SharedPtr<rdr::TextureView> m_temporalMomentsHistory;

	Bool m_hasValidHistory;
};

} // visibility_denoiser

} // spt::rsc
