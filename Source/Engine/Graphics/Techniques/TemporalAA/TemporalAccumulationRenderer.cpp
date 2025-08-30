#include "TemporalAccumulationRenderer.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "MathUtils.h"
#include "Common/ShaderCompilationInput.h"
#include "Bindless/BindlessTypes.h"
#include "DescriptorSetBindings/RWBufferBinding.h"


namespace spt::gfx
{

BEGIN_SHADER_STRUCT(TemporalAccumulationConstants)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector4f>, input)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector4f>, accumulatedData)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector4f>, output)
	SHADER_STRUCT_FIELD(Real32, currentFrameWeight)
	SHADER_STRUCT_FIELD(Real32, historyWeight)
	SHADER_STRUCT_FIELD(Uint32, exposureOffset)
	SHADER_STRUCT_FIELD(Uint32, historyExposureOffset)
END_SHADER_STRUCT();


DS_BEGIN(TemporalAccumulationDS, rg::RGDescriptorSetState<TemporalAccumulationDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TemporalAccumulationConstants>), u_constants)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Real32>),                      u_exposure)
DS_END();


static rdr::PipelineStateID GetTemporalAccumulationPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/AntiAliasing/TemporalAccumulation.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TemporalAccumulationCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TemporalAccumulationPipeline"), shader);
}

TemporalAccumulationRenderer::TemporalAccumulationRenderer()
{
	m_name = "Temporal Accumulation";
	m_executesUnifiedDenoising = true;
}

Bool TemporalAccumulationRenderer::Initialize(const TemporalAAInitSettings& initSettings)
{
	Super::Initialize(initSettings);

	return true;
}

math::Vector2f TemporalAccumulationRenderer::ComputeJitter(Uint64 frameIdx, math::Vector2u renderingResolution, math::Vector2u outputResolution) const
{
	const Uint32 sequenceLength = 8u;
	return TemporalAAJitterSequence::Halton(frameIdx, renderingResolution, sequenceLength);
}

Bool TemporalAccumulationRenderer::PrepareForRendering(const TemporalAAParams& params)
{
	SPT_PROFILER_FUNCTION();

	if (!Super::PrepareForRendering(params))
	{
		return false;
	}

	SPT_CHECK(params.inputResolution == params.outputResolution);

	const math::Vector2u renderingResolution = params.inputResolution;

	if (!m_accumulationTexture || m_accumulationTexture->GetResolution2D() != renderingResolution)
	{
		rhi::TextureDefinition taaTextureDef;
		taaTextureDef.resolution = renderingResolution;
		taaTextureDef.usage      = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::TransferDest);
		taaTextureDef.format     = rhi::EFragmentFormat::RGBA32_S_Float;
		m_accumulationTexture    = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Accumulation Texture"), taaTextureDef, rhi::EMemoryUsage::GPUOnly);

		m_historyLength = 0u;
	}

	return true;
}

void TemporalAccumulationRenderer::Render(rg::RenderGraphBuilder& graphBuilder, const TemporalAARenderingParams& renderingParams)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = renderingParams.inputColor->GetResolution2D();

	const rg::RGTextureViewHandle accumulationTexture = graphBuilder.AcquireExternalTextureView(m_accumulationTexture);

	const UnifiedDenoisingParams& denoisingParams = renderingParams.unifiedDenoisingParams.value();
	if (m_lastWorldToView != denoisingParams.worldToView)
	{
		m_historyLength = 0u;
	}
	m_lastWorldToView = denoisingParams.worldToView;

	const Uint32 historyLength = m_historyLength++;
	if (historyLength == 0u)
	{
		graphBuilder.ClearTexture(RG_DEBUG_NAME("Clear Accumulation Texture"), accumulationTexture, rhi::ClearColor(0.f, 0.f, 0.f, 0.f));
	}

	const Real32 currentFrameWeight = 1.0f / static_cast<Real32>(historyLength + 1);
	const Real32 historyWeight = 1.0f - currentFrameWeight;

	TemporalAccumulationConstants shaderConstants;
	shaderConstants.input                 = renderingParams.inputColor;
	shaderConstants.accumulatedData       = accumulationTexture;
	shaderConstants.output                = renderingParams.outputColor;
	shaderConstants.currentFrameWeight    = currentFrameWeight;
	shaderConstants.historyWeight         = historyWeight;
	shaderConstants.exposureOffset        = renderingParams.exposure.exposureOffset / sizeof(Real32);
	shaderConstants.historyExposureOffset = renderingParams.exposure.historyExposureOffset / sizeof(Real32);

	const lib::MTHandle<TemporalAccumulationDS> ds = graphBuilder.CreateDescriptorSet<TemporalAccumulationDS>(RENDERER_RESOURCE_NAME("Temporal Accumulation DS"));
	ds->u_constants = shaderConstants;
	ds->u_exposure  = renderingParams.exposure.exposureBuffer;

	static rdr::PipelineStateID pipeline = GetTemporalAccumulationPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Temporal Accumulation"),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(ds));
}

} // spt::gfx
