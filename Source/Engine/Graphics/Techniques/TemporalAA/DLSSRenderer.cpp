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
	m_name              = "DLSS";
	m_supportsUpscaling = true;
}

Bool DLSSRenderer::Initialize(const TemporalAAInitSettings& initSettings)
{
	return m_dlssBackend.Initialize();
}

math::Vector2f DLSSRenderer::ComputeJitter(Uint64 frameIdx, math::Vector2u renderingResolution, math::Vector2u outputResolution) const
{
	// See DLSS Programing Guide, section 3.7.1.1
	const Uint32 renderingArea       = renderingResolution.x() * renderingResolution.y();
	const Uint32 outputArea          = outputResolution.x() * outputResolution.y();
	const Real32 renderingPixelScale = outputArea / static_cast<Real32>(renderingArea);

	const Uint32 standardSequenceLength = 8u;

	const Uint32 sequenceLength = math::Utils::RoundUpToPowerOf2(static_cast<Uint32>(standardSequenceLength * renderingPixelScale));

	return 0.5f * TemporalAAJitterSequence::Halton(frameIdx, renderingResolution, sequenceLength);
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