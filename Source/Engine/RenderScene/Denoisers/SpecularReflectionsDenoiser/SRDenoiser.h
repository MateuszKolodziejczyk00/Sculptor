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

		rg::RGTextureViewHandle specularColorAndRoughnessTexture;

		rg::RGTextureViewHandle motionTexture;

		rg::RGTextureViewHandle normalsTexture;

		lib::MTHandle<ddgi::DDGISceneDS> ddgiSceneDS;
	};

	explicit Denoiser(rg::RenderGraphDebugName debugName);

	void Denoise(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params);

private:

	void UpdateResources(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params);

	void DenoiseImpl(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, const Params& params);

	void ApplySpatialFilter(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle denoisedTexture, rg::RGTextureViewHandle historyTexture, rg::RGTextureViewHandle stdDevTexture, const Params& params);

	rg::RenderGraphDebugName m_debugName;

	lib::SharedPtr<rdr::TextureView> m_historyTexture;
	
	lib::SharedPtr<rdr::TextureView> m_accumulatedSamplesNumTexture;
	lib::SharedPtr<rdr::TextureView> m_historyAccumulatedSamplesNumTexture;
	
	lib::SharedPtr<rdr::TextureView> m_temporalVarianceTexture;
	lib::SharedPtr<rdr::TextureView> m_historyTemporalVarianceTexture;

	Bool m_hasValidHistory;
};

} // spt::rsc::sr_denoiser