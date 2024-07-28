#include "VariableRateTexture.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"


namespace spt::rsc::vrt
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utils =========================================================================================

void ApplyVariableRatePermutation(sc::ShaderCompilationSettings& compilationSettings, const VariableRatePermutationSettings& permutationSettings)
{
	const char* variableRateValue = permutationSettings.maxVariableRate == EMaxVariableRate::_2x2 ? "0" : "1";
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("SPT_VARIABLE_RATE_MODE", variableRateValue));
}

static math::Vector2u GetVRTileSize(EVRTileSize tileSize)
{
	switch (tileSize)
	{
	case EVRTileSize::_2x2:
		return math::Vector2u::Constant(2u);
	case EVRTileSize::_4x4:
		return math::Vector2u::Constant(4u);
	default:
		SPT_CHECK_NO_ENTRY();
		return math::Vector2u::Constant(0u);
	}
}

math::Vector2u ComputeVariableRateTextureResolution(const math::Vector2u& inputTextureResolution, EVRTileSize tile)
{
	const math::Vector2u tileSize = GetVRTileSize(tile);
	return math::Utils::DivideCeil(inputTextureResolution, tileSize);
}

BEGIN_SHADER_STRUCT(CreateVariableRateTextureConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, inputResolution)
	SHADER_STRUCT_FIELD(Uint32,         inputSignalType)
	SHADER_STRUCT_FIELD(Real32,         xThreshold2)
	SHADER_STRUCT_FIELD(Real32,         yThreshold2)
	SHADER_STRUCT_FIELD(Real32,         xThreshold4)
	SHADER_STRUCT_FIELD(Real32,         yThreshold4)
	SHADER_STRUCT_FIELD(Uint32,         tileSize)
	SHADER_STRUCT_FIELD(Uint32,         signalType)
	SHADER_STRUCT_FIELD(Uint32,         frameIdx)
END_SHADER_STRUCT();


DS_BEGIN(CreateVariableRateTextureDS, rg::RGDescriptorSetState<CreateVariableRateTextureDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                       u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                                u_rwVariableRateTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<CreateVariableRateTextureConstants>), u_constants)
DS_END();


static rdr::PipelineStateID CreateVariableRateTexturePipeline(const VariableRatePermutationSettings& permutationSettings)
{
	sc::ShaderCompilationSettings compilationSettings;
	ApplyVariableRatePermutation(INOUT compilationSettings, permutationSettings);
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/VariableRate/CreateVariableRateTexture.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CreateVariableRateTextureCS"), compilationSettings);
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("CreateVariableRateTexturePipeline"), shader);
}

void RenderVariableRateTexture(rg::RenderGraphBuilder& graphBuilder, const VariableRateSettings& vrSettings, rg::RGTextureViewHandle inputTexture, rg::RGTextureViewHandle variableRateTexture, Uint32 frameIdx)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(inputTexture.IsValid());
	SPT_CHECK(variableRateTexture.IsValid());

	CreateVariableRateTextureConstants shaderConstants;
	shaderConstants.inputResolution = inputTexture->GetResolution2D();
	shaderConstants.inputSignalType = static_cast<Uint32>(vrSettings.signalType);
	shaderConstants.xThreshold2     = vrSettings.xThreshold2;
	shaderConstants.yThreshold2     = vrSettings.yThreshold2;
	shaderConstants.xThreshold4     = vrSettings.xThreshold4;
	shaderConstants.yThreshold4     = vrSettings.yThreshold4;
	shaderConstants.tileSize        = static_cast<Uint32>(vrSettings.tileSize);
	shaderConstants.signalType      = static_cast<Uint32>(vrSettings.signalType);
	shaderConstants.frameIdx        = frameIdx;

	lib::MTHandle<CreateVariableRateTextureDS> variableRateTextureDS = graphBuilder.CreateDescriptorSet<CreateVariableRateTextureDS>(RENDERER_RESOURCE_NAME("VariableRateTextureDS"));
	variableRateTextureDS->u_inputTexture          = inputTexture;
	variableRateTextureDS->u_rwVariableRateTexture = variableRateTexture;
	variableRateTextureDS->u_constants             = shaderConstants;

	const rdr::PipelineStateID variableRateTexturePipeline = CreateVariableRateTexturePipeline(vrSettings.permutationSettings);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Create Variable Rate Texture"),
						  variableRateTexturePipeline,
						  math::Utils::DivideCeil(shaderConstants.inputResolution, math::Vector2u(8u, 4u)),
						  rg::BindDescriptorSets(std::move(variableRateTextureDS)));
}


BEGIN_SHADER_STRUCT(ReprojectVariableRateTextureConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
END_SHADER_STRUCT();


DS_BEGIN(ReprojectVariableRateTextureDS, rg::RGDescriptorSetState<ReprojectVariableRateTextureDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                                   u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                                    u_rwOutputTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_motionTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<ReprojectVariableRateTextureConstants>),  u_constants)
DS_END();


static rdr::PipelineStateID CreateReprojectVariableRateTexturePipeline(const VariableRatePermutationSettings& permutationSettings)
{
	sc::ShaderCompilationSettings compilationSettings;
	ApplyVariableRatePermutation(INOUT compilationSettings, permutationSettings);
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/VariableRate/ReprojectVariableRateTexture.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ReprojectVariableRateTextureCS"), compilationSettings);
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("ReprojectVariableRateTexturePipeline"), shader);
}

void ReprojectVariableRateTexture(rg::RenderGraphBuilder& graphBuilder, const VariableRateSettings& vrSettings, rg::RGTextureViewHandle motionTexture, rg::RGTextureViewHandle sourceTexture, rg::RGTextureViewHandle targetTexture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(motionTexture.IsValid());
	SPT_CHECK(sourceTexture.IsValid());
	SPT_CHECK(targetTexture.IsValid());

	const math::Vector2u resolution = sourceTexture->GetResolution2D();

	ReprojectVariableRateTextureConstants shaderConstants;
	shaderConstants.resolution    = resolution;
	shaderConstants.invResolution = resolution.cast<Real32>().cwiseInverse();

	lib::MTHandle<ReprojectVariableRateTextureDS> reprojectVariableRateTextureDS = graphBuilder.CreateDescriptorSet<ReprojectVariableRateTextureDS>(RENDERER_RESOURCE_NAME("ReprojectVariableRateTextureDS"));
	reprojectVariableRateTextureDS->u_inputTexture    = sourceTexture;
	reprojectVariableRateTextureDS->u_rwOutputTexture = targetTexture;
	reprojectVariableRateTextureDS->u_motionTexture   = motionTexture;
	reprojectVariableRateTextureDS->u_constants       = shaderConstants;

	const rdr::PipelineStateID reprojectVariableRateTexturePipeline = CreateReprojectVariableRateTexturePipeline(vrSettings.permutationSettings);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Reproject Variable Rate Texture"),
						  reprojectVariableRateTexturePipeline,
						  math::Utils::DivideCeil(shaderConstants.resolution, math::Vector2u(8u, 4u)),
						  rg::BindDescriptorSets(std::move(reprojectVariableRateTextureDS)));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// VariableRateRenderer ==========================================================================

VariableRateRenderer::VariableRateRenderer()
{
}

void VariableRateRenderer::Initialize(const VariableRateSettings& vrSettings)
{
	m_vrSettings = vrSettings;
}

void VariableRateRenderer::Reproject(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle motionTexture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(motionTexture.IsValid());

	const Bool canPerformReprojection = PrepareTextures(graphBuilder, motionTexture);

	const rg::RGTextureViewHandle reprojectionTarget = graphBuilder.AcquireExternalTextureView(m_reprojectionTargetVRTexture);

	if (canPerformReprojection)
	{
		const rg::RGTextureViewHandle reprojectionSource = graphBuilder.AcquireExternalTextureView(m_reprojectionSourceVRTexture);
		ReprojectVariableRateTexture(graphBuilder, m_vrSettings, motionTexture, reprojectionSource, reprojectionTarget);
	}
	else
	{
		graphBuilder.ClearTexture(RG_DEBUG_NAME("Clear Variable Rate Texture"), reprojectionTarget, rhi::ClearColor(0u, 0u, 0u, 0u));
	}
}

void VariableRateRenderer::Render(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle inputTexture)
{
	SPT_PROFILER_FUNCTION();

	std::swap(m_reprojectionSourceVRTexture, m_reprojectionTargetVRTexture);

	const rg::RGTextureViewHandle variableRateTexture = graphBuilder.AcquireExternalTextureView(m_reprojectionSourceVRTexture);

	RenderVariableRateTexture(graphBuilder, m_vrSettings, inputTexture, variableRateTexture, m_frameIdx);

	m_frameIdx++;
}

Bool VariableRateRenderer::PrepareTextures(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle inputTexture)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u variableRateTextureResolution = ComputeVariableRateTextureResolution(inputTexture->GetResolution2D(), m_vrSettings.tileSize);

	const EMaxVariableRate maxVariableRate = m_vrSettings.permutationSettings.maxVariableRate;

	const rhi::EFragmentFormat variableRateTextureFormat = maxVariableRate == EMaxVariableRate::_2x2
		                                                 ? rhi::EFragmentFormat::R8_U_Int
		                                                 : rhi::EFragmentFormat::R16_U_Int;

	const auto updateTextureIfNecessary = [variableRateTextureResolution, variableRateTextureFormat](lib::SharedPtr<rdr::TextureView>& texture) -> Bool
		{
			Bool textureUpdated = false;
			if (!texture || texture->GetResolution2D() != variableRateTextureResolution)
			{
				rhi::TextureDefinition textureDef;
				textureDef.resolution = variableRateTextureResolution;
				textureDef.format     = variableRateTextureFormat;
				textureDef.usage      = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest);
				texture = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("VariableRateTexture"), textureDef, rhi::EMemoryUsage::GPUOnly);

				textureUpdated = true;
			}

			return textureUpdated;
		};

	Bool anyTextureUpdated = false;

	anyTextureUpdated |= updateTextureIfNecessary(m_reprojectionSourceVRTexture);
	anyTextureUpdated |= updateTextureIfNecessary(m_reprojectionTargetVRTexture);
	
	return !anyTextureUpdated;
}

} // spt::rsc::vrt
