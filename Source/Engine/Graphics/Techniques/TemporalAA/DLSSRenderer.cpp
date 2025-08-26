#include "DLSSRenderer.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
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

dlss::EDLSSQuality ComputeDLSSQuality(const TemporalAAParams& params)
{
	if (params.inputResolution == params.outputResolution)
	{
		return dlss::EDLSSQuality::DLAA;
	}
	else
	{
		switch (params.quality)
		{
		case ETemporalAAQuality::Low:    return dlss::EDLSSQuality::Low;
		case ETemporalAAQuality::Medium: return dlss::EDLSSQuality::Medium;
		case ETemporalAAQuality::Ultra:  return dlss::EDLSSQuality::Ultra;
		default:
			SPT_CHECK_NO_ENTRY_MSG("Invalid temporal AA quality");
			return dlss::EDLSSQuality::Ultra;
		}
	}
}

dlss::EDLSSFlags ComputeDLSSFlags(const TemporalAAParams& params)
{
	dlss::EDLSSFlags flags = dlss::EDLSSFlags::None;

	if (lib::HasAnyFlag(params.flags, ETemporalAAFlags::HDR))
	{
		lib::AddFlag(flags, dlss::EDLSSFlags::HDR);
	}

	if (lib::HasAnyFlag(params.flags, ETemporalAAFlags::DepthInverted))
	{
		lib::AddFlag(flags, dlss::EDLSSFlags::ReversedDepth);
	}

	if (lib::HasAnyFlag(params.flags, ETemporalAAFlags::MotionLowRes))
	{
		lib::AddFlag(flags, dlss::EDLSSFlags::MotionLowRes);
	}

	return flags;
}

} // priv

DLSSRenderer::DLSSRenderer()
{
	m_name				       = "DLSS";
	m_supportsUpscaling	       = true;
	m_executesUnifiedDenoising = false;
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

	Uint32 sequenceLength = math::Utils::RoundUpToPowerOf2(static_cast<Uint32>(standardSequenceLength * renderingPixelScale));

	if (m_executesUnifiedDenoising)
	{
		sequenceLength = std::max(sequenceLength, 64u);
	}

	return 0.5f * TemporalAAJitterSequence::Halton(frameIdx, renderingResolution, sequenceLength);
}

Bool DLSSRenderer::PrepareForRendering(const TemporalAAParams& params)
{
	if (!Super::PrepareForRendering(params))
	{
		return false;
	}

	dlss::DLSSParams dlssParams;
	dlssParams.inputResolution         = params.inputResolution;
	dlssParams.outputResolution        = params.outputResolution;
	dlssParams.quality                 = priv::ComputeDLSSQuality(params);
	dlssParams.flags                   = priv::ComputeDLSSFlags(params);
	dlssParams.enableRayReconstruction = params.enableUnifiedDenoising;
	dlssParams.enableTransformerModel  = params.enableTransformerModel;

	const Bool success = m_dlssBackend.PrepareForRendering(dlssParams);

	m_executesUnifiedDenoising = success && params.enableUnifiedDenoising;

	return success;
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

	if (renderingParams.unifiedDenoisingParams)
	{
		const UnifiedDenoisingParams& unifiedDenoisingParams = *renderingParams.unifiedDenoisingParams;

		dlss::DLSSRayReconstructionRenderingParams rayReconstructionParams;
		rayReconstructionParams.worldToView         = unifiedDenoisingParams.worldToView;
		rayReconstructionParams.viewToClip         = unifiedDenoisingParams.viewToClip;
		rayReconstructionParams.diffuseAlbedo       = unifiedDenoisingParams.diffuseAlbedo;
		rayReconstructionParams.specularAlbedo      = unifiedDenoisingParams.specularAlbedo;
		rayReconstructionParams.normals             = unifiedDenoisingParams.normals;
		rayReconstructionParams.roughness           = unifiedDenoisingParams.roughness;
		rayReconstructionParams.specularHitDistance = unifiedDenoisingParams.specularHitDistance;

		dlssRenderingParams.rayReconstructionParams = rayReconstructionParams;
	}

	m_dlssBackend.Render(graphBuilder, dlssRenderingParams);
}

} // spt::gfx