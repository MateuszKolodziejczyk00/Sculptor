#include "StandardTAARenderer.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "ResourcesManager.h"
#include "MathUtils.h"
#include "Common/ShaderCompilationInput.h"


namespace spt::gfx
{

BEGIN_SHADER_STRUCT(TemporalAAConstants)
	SHADER_STRUCT_FIELD(Uint32, useYCoCg)
	SHADER_STRUCT_FIELD(Uint32, exposureOffset)
	SHADER_STRUCT_FIELD(Uint32, historyExposureOffset)
END_SHADER_STRUCT();


DS_BEGIN(TemporalAADS, rg::RGDescriptorSetState<TemporalAADS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                    u_depth)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                            u_historyColor)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                            u_inputColor)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                            u_motion)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                             u_outputColor)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),  u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Real32>),                                u_exposure)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TemporalAAConstants>),                     u_params)
DS_END();


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
	Super::Initialize(initSettings);

	return true;
}

math::Vector2f StandardTAARenderer::ComputeJitter(Uint64 frameIdx, math::Vector2u resolution) const
{
	return TemporalAAJitterSequence::Halton(frameIdx, resolution);
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

		const lib::MTHandle<TemporalAADS> temporalAADS = graphBuilder.CreateDescriptorSet<TemporalAADS>(RENDERER_RESOURCE_NAME("Temporal AA DS"));
		temporalAADS->u_depth           = renderingParams.depth;
		temporalAADS->u_inputColor      = renderingParams.inputColor;
		temporalAADS->u_historyColor    = historyTexture;
		temporalAADS->u_motion          = renderingParams.motion;
		temporalAADS->u_outputColor     = renderingParams.outputColor;
		temporalAADS->u_exposure        = renderingParams.exposure.exposureBuffer;
		temporalAADS->u_params          = shaderConstants;

		static rdr::PipelineStateID temporalAAPipelineStateID = GetTemporalAAPipeline();

		graphBuilder.Dispatch(RG_DEBUG_NAME("Temporal AA"),
							  temporalAAPipelineStateID,
							  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
							  rg::BindDescriptorSets(temporalAADS));

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
