#include "DLSSRenderer.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"


namespace spt::gfx
{

namespace priv
{

rg::RGTextureViewHandle PrepareExposureTexture(rg::RenderGraphBuilder& graphBuilder, const TemporalAARenderingParams& renderingParams)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle exposureTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Exposure Texture"), rg::TextureDef(math::Vector2u(1u, 1u), rhi::EFragmentFormat::R32_S_Float));


	graphBuilder.CopyBufferToFullTexture(RG_DEBUG_NAME("Copy Exposure To Texture"),
										 renderingParams.exposure.exposureBuffer, renderingParams.exposure.exposureOffset,
										 exposureTexture);
	return exposureTexture;
}

} // priv

DLSSRenderer::DLSSRenderer()
{
	m_name = "DLSS";
}

Bool DLSSRenderer::Initialize(const TemporalAAInitSettings& initSettings)
{
	return m_dlssBackend.Initialize();
}

math::Vector2f DLSSRenderer::ComputeJitter(Uint64 frameIdx, math::Vector2u resolution) const
{
	return TemporalAAJitterSequence::Halton(frameIdx, resolution);
}

Bool DLSSRenderer::PrepareForRendering(const TemporalAAParams& params)
{
	if (!Super::PrepareForRendering(params))
	{
		return false;
	}

	dlss::DLSSParams dlssParams;
	dlssParams.inputResolution  = params.inputResolution;
	dlssParams.outputResolution = params.outputResolution;

	return m_dlssBackend.PrepareForRendering(dlssParams);
}

void DLSSRenderer::Render(rg::RenderGraphBuilder& graphBuilder, const TemporalAARenderingParams& renderingParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "DLSS");

	const rg::RGTextureViewHandle exposureTexture = priv::PrepareExposureTexture(graphBuilder, renderingParams);

	dlss::DLSSRenderingParams dlssRenderingParams;
	dlssRenderingParams.depth             = renderingParams.depth;
	dlssRenderingParams.motion            = renderingParams.motion;
	dlssRenderingParams.inputColor        = renderingParams.inputColor;
	dlssRenderingParams.outputColor       = renderingParams.outputColor;
	dlssRenderingParams.exposure          = exposureTexture;
	dlssRenderingParams.jitter            = renderingParams.jitter;
	dlssRenderingParams.sharpness         = renderingParams.sharpness;
	dlssRenderingParams.resetAccumulation = renderingParams.resetAccumulation;

	m_dlssBackend.Render(graphBuilder, dlssRenderingParams);
}

} // spt::gfx