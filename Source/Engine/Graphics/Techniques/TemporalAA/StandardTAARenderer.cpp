#include "StandardTAARenderer.h"
#include "RenderGraphBuilder.h"
#include "Bindless/BindlessTypes.h"
#include "ResourcesManager.h"
#include "MathUtils.h"
#include "Common/ShaderCompilationInput.h"


namespace spt::gfx
{

BEGIN_SHADER_STRUCT(TemporalAAConstants)
	SHADER_STRUCT_FIELD(Uint32,                               useYCoCg)
	SHADER_STRUCT_FIELD(Uint32,                               exposureOffset)
	SHADER_STRUCT_FIELD(Uint32,                               historyExposureOffset)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,         depth)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector3f>, historyColor)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector3f>, inputColor)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>, motion)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector3f>, outputColor)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<Real32>,          exposure)
END_SHADER_STRUCT();


static rdr::PipelineStateID GetTemporalAAPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/AntiAliasing/TemporalAA.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TemporalAACS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TemporalAAPipeline"), shader);
}

StandardTAARenderer::StandardTAARenderer()
{
	m_name = "Standard TAA";
}

Bool StandardTAARenderer::Initialize(const TemporalAAInitSettings& initSettings)
{
	return true;
}

math::Vector2f StandardTAARenderer::ComputeJitter(Uint64 frameIdx, math::Vector2u renderingResolution, math::Vector2u outputResolution) const
{
	const Uint32 sequenceLength = 8u;
	return TemporalAAJitterSequence::Halton(frameIdx, renderingResolution, sequenceLength);
}

Bool StandardTAARenderer::PrepareForRendering(const TemporalAAParams& params)
{
	SPT_PROFILER_FUNCTION();

	if (!Super::PrepareForRendering(params))
	{
		return false;
	}

	SPT_CHECK(params.inputResolution == params.outputResolution);

	const math::Vector2u renderingResolution = params.inputResolution;

	if (!m_historyTextureView || m_historyTextureView->GetResolution2D() != renderingResolution)
	{
		rhi::TextureDefinition taaTextureDef;
		taaTextureDef.resolution = renderingResolution;
		taaTextureDef.usage      = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::TransferDest);
		taaTextureDef.format     = rhi::EFragmentFormat::RGBA16_S_Float;
		lib::SharedRef<rdr::Texture> outputTexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("TAA Texture"), taaTextureDef, rhi::EMemoryUsage::GPUOnly);

		rhi::TextureViewDefinition viewDef;
		viewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		m_historyTextureView = outputTexture->CreateView(RENDERER_RESOURCE_NAME("TAA Texture View"), viewDef);
		m_hasValidHistory = false;
	}

	return true;
}

void StandardTAARenderer::Render(rg::RenderGraphBuilder& graphBuilder, const TemporalAARenderingParams& renderingParams)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = renderingParams.inputColor->GetResolution2D();

	const rg::RGTextureViewHandle historyTexture = graphBuilder.AcquireExternalTextureView(m_historyTextureView);

	if (m_historyTextureView && m_historyTextureView->GetResolution2D() == resolution)
	{
		SPT_CHECK((renderingParams.exposure.exposureOffset & (sizeof(Uint32) - 1u)) == 0u);
		SPT_CHECK((renderingParams.exposure.historyExposureOffset & (sizeof(Uint32) - 1u)) == 0u);

		TemporalAAConstants shaderConstants;
		shaderConstants.useYCoCg              = false;
		shaderConstants.exposureOffset        = renderingParams.exposure.exposureOffset / sizeof(Uint32);
		shaderConstants.historyExposureOffset = renderingParams.exposure.historyExposureOffset / sizeof(Uint32);
		shaderConstants.depth                 = renderingParams.depth;
		shaderConstants.inputColor            = renderingParams.inputColor;
		shaderConstants.historyColor          = historyTexture;
		shaderConstants.motion                = renderingParams.motion;
		shaderConstants.outputColor           = renderingParams.outputColor;
		shaderConstants.exposure              = renderingParams.exposure.exposureBuffer;

		static rdr::PipelineStateID temporalAAPipelineStateID = GetTemporalAAPipeline();

		graphBuilder.Dispatch(RG_DEBUG_NAME("Temporal AA"),
							  temporalAAPipelineStateID,
							  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
							  rg::ShaderParams(shaderConstants));

	}
	else
	{
		// We don't have history so we just copy the current frame
		graphBuilder.CopyTexture(RG_DEBUG_NAME("Save Temporal AA history"),
								 renderingParams.inputColor, math::Vector3i::Zero(),
								 renderingParams.outputColor, math::Vector3i::Zero(),
								 math::Vector3u(resolution.x(), resolution.y(), 1u));
	}

	// Cache texture for next frame
	graphBuilder.CopyTexture(RG_DEBUG_NAME("Apply Temporal AA result"),
							 renderingParams.outputColor, math::Vector3i::Zero(),
							 historyTexture, math::Vector3i::Zero(),
							 math::Vector3u(resolution.x(), resolution.y(), 1u));
	
	m_hasValidHistory = true;
}

} // spt::gfx
