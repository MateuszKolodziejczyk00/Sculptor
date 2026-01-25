#include "VariableRateTexture.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "View/RenderView.h"


namespace spt::rsc::vrt
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utils =========================================================================================

void ApplyVariableRatePermutation(sc::ShaderCompilationSettings& compilationSettings, const VariableRatePermutationSettings& permutationSettings)
{
	const char* variableRateValue = permutationSettings.maxVariableRate == EMaxVariableRate::_2x2 ? "0" : "1";
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("SPT_VARIABLE_RATE_MODE", variableRateValue));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("VR_USE_LARGE_TILE", permutationSettings.useLargeTile ? "1" : "0"));
}

math::Vector2u ComputeVariableRateTextureResolution(const math::Vector2u& inputTextureResolution, const VariableRateSettings& vrSettings)
{
	const math::Vector2u tileSize = math::Vector2u::Constant(GetTileSize(vrSettings));
	return math::Utils::DivideCeil(inputTextureResolution, tileSize);
}

math::Vector2u ComputeReprojectionSuccessMaskResolution(const math::Vector2u& inputTextureResolution, const VariableRateSettings& vrSettings)
{
	return math::Utils::DivideCeil(ComputeVariableRateTextureResolution(inputTextureResolution, vrSettings), math::Vector2u(8u, 4u));
}

rg::RGTextureViewHandle CreateReprojectionSuccessMask(rg::RenderGraphBuilder& graphBuilder, const math::Vector2u& inputTextureResolution, const VariableRateSettings& vrSettings)
{
	const math::Vector2u resolution = ComputeReprojectionSuccessMaskResolution(inputTextureResolution, vrSettings);
	return graphBuilder.CreateTextureView(RG_DEBUG_NAME("Reprojection Success Mask"), rg::TextureDef(resolution, rhi::EFragmentFormat::R32_U_Int));
}


BEGIN_SHADER_STRUCT(CreateVariableRateTextureConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, inputResolution)
	SHADER_STRUCT_FIELD(math::Vector2u, outputResolution)
	SHADER_STRUCT_FIELD(Uint32,         inputSignalType)
	SHADER_STRUCT_FIELD(Real32,         xThreshold2)
	SHADER_STRUCT_FIELD(Real32,         yThreshold2)
	SHADER_STRUCT_FIELD(Real32,         xThreshold4)
	SHADER_STRUCT_FIELD(Real32,         yThreshold4)
	SHADER_STRUCT_FIELD(Uint32,         logFramesNumPerSlot)
	SHADER_STRUCT_FIELD(Uint32,         frameIdx)
END_SHADER_STRUCT();


DS_BEGIN(CreateVariableRateTextureDS, rg::RGDescriptorSetState<CreateVariableRateTextureDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                            u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                                     u_rwVariableRateTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<CreateVariableRateTextureConstants>),      u_constants)
DS_END();


static rdr::ShaderID CreateGenericVariableRateTextureShader(const VariableRatePermutationSettings& permutationSettings)
{
	sc::ShaderCompilationSettings compilationSettings;
	ApplyVariableRatePermutation(INOUT compilationSettings, permutationSettings);
	return rdr::ResourcesManager::CreateShader("Sculptor/Utils/VariableRate/CreateGenericVariableRateTexture.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CreateVariableRateTextureCS"), compilationSettings);
}

static void RenderVariableRateTexture(rg::RenderGraphBuilder& graphBuilder, const VariableRateSettings& vrSettings, rg::RGTextureViewHandle inputTexture, math::Vector2u inputResolution, rg::RGTextureViewHandle variableRateTexture, Uint32 frameIdx, std::optional<rdr::ShaderID> customShader, lib::Span<const lib::MTHandle<rg::RGDescriptorSetStateBase>> additionalDescriptorSets)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(variableRateTexture.IsValid());

	CreateVariableRateTextureConstants shaderConstants;
	shaderConstants.inputResolution     = inputResolution;
	shaderConstants.outputResolution    = variableRateTexture->GetResolution2D();
	shaderConstants.xThreshold2         = vrSettings.xThreshold2;
	shaderConstants.yThreshold2         = vrSettings.yThreshold2;
	shaderConstants.xThreshold4         = vrSettings.xThreshold4;
	shaderConstants.yThreshold4         = vrSettings.yThreshold4;
	shaderConstants.frameIdx            = frameIdx;
	shaderConstants.logFramesNumPerSlot = vrSettings.logFramesNumPerSlot;

	lib::MTHandle<CreateVariableRateTextureDS> variableRateTextureDS = graphBuilder.CreateDescriptorSet<CreateVariableRateTextureDS>(RENDERER_RESOURCE_NAME("VariableRateTextureDS"));
	variableRateTextureDS->u_rwVariableRateTexture = variableRateTexture;
	variableRateTextureDS->u_constants             = shaderConstants;

	if (inputTexture.IsValid())
	{
		variableRateTextureDS->u_inputTexture = inputTexture;
	}

	rdr::ShaderID shader;
	if (customShader.has_value())
	{
		shader = customShader.value();
	}
	else
	{
		shader = CreateGenericVariableRateTextureShader(vrSettings.permutationSettings);
	}

	SPT_CHECK(shader.IsValid());

	const rg::BindDescriptorSetsScope additionalDescriptorSetsScope(graphBuilder, additionalDescriptorSets);

	const math::Vector2u groupSize = vrSettings.variableRateBuilderUseSingleLanePerQuad ? math::Vector2u(16u, 16u) : math::Vector2u(8u, 8u);

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} - Create Variable Rate Texture", vrSettings.debugName.AsString()),
						  shader,
						  math::Utils::DivideCeil(shaderConstants.inputResolution, groupSize),
						  rg::BindDescriptorSets(std::move(variableRateTextureDS)));
}


BEGIN_SHADER_STRUCT(ReprojectVariableRateTextureConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invResolution)
	SHADER_STRUCT_FIELD(Uint32,         reprojectionFailedMode)
	SHADER_STRUCT_FIELD(Real32,         reprojectionMaxPlaneDistance)
END_SHADER_STRUCT();


DS_BEGIN(ReprojectVariableRateTextureDS, rg::RGDescriptorSetState<ReprojectVariableRateTextureDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                                   u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                                    u_rwOutputTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<Uint32>),                            u_rwReprojectionSuccessMask)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),                           u_motionTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                           u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<math::Vector2f>),                   u_normalsTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),                           u_historyDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<ReprojectVariableRateTextureConstants>),  u_constants)
DS_END();


static rdr::PipelineStateID CreateReprojectVariableRateTexturePipeline(const VariableRatePermutationSettings& permutationSettings, Bool outputReprojectionSuccessMask, Bool useDepthTest)
{
	sc::ShaderCompilationSettings compilationSettings;
	ApplyVariableRatePermutation(INOUT compilationSettings, permutationSettings);
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("OUTPUT_REPROJECTION_SUCCESS_MASK", outputReprojectionSuccessMask ? "1" : "0"));
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("USE_DEPTH_TEST", useDepthTest ? "1" : "0"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/VariableRate/ReprojectVariableRateTexture.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ReprojectVariableRateTextureCS"), compilationSettings);
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("ReprojectVariableRateTexturePipeline"), shader);
}

static void ReprojectVariableRateTexture(rg::RenderGraphBuilder& graphBuilder, const VariableRateSettings& vrSettings, rg::RGTextureViewHandle sourceTexture, rg::RGTextureViewHandle targetTexture, const VariableRateReprojectionParams& reprojectionParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(reprojectionParams.motionTexture.IsValid());
	SPT_CHECK(sourceTexture.IsValid());
	SPT_CHECK(targetTexture.IsValid());

	const math::Vector2u resolution = sourceTexture->GetResolution2D();

	ReprojectVariableRateTextureConstants shaderConstants;
	shaderConstants.resolution             = resolution;
	shaderConstants.invResolution          = resolution.cast<Real32>().cwiseInverse();
	shaderConstants.reprojectionFailedMode = static_cast<Uint32>(vrSettings.reprojectionFailedMode);

	const Bool outputReprojectionSuccessMask = reprojectionParams.reprojectionSuccessMask.IsValid();
	const Bool useDepthTest                  = reprojectionParams.geometryData.has_value();

	lib::MTHandle<ReprojectVariableRateTextureDS> reprojectVariableRateTextureDS = graphBuilder.CreateDescriptorSet<ReprojectVariableRateTextureDS>(RENDERER_RESOURCE_NAME("ReprojectVariableRateTextureDS"));
	reprojectVariableRateTextureDS->u_inputTexture              = sourceTexture;
	reprojectVariableRateTextureDS->u_rwOutputTexture           = targetTexture;
	reprojectVariableRateTextureDS->u_motionTexture             = reprojectionParams.motionTexture;

	if (outputReprojectionSuccessMask)
	{
		reprojectVariableRateTextureDS->u_rwReprojectionSuccessMask = reprojectionParams.reprojectionSuccessMask;
	}

	if (useDepthTest)
	{
		const VariableRateReprojectionParams::Geometry& geometryData = reprojectionParams.geometryData.value();

		SPT_CHECK(geometryData.currentDepth.IsValid());
		SPT_CHECK(geometryData.currentNormals.IsValid());
		SPT_CHECK(geometryData.historyDepth.IsValid());

		SPT_CHECK(!!geometryData.renderView);
		
		reprojectVariableRateTextureDS->u_depthTexture        = geometryData.currentDepth;
		reprojectVariableRateTextureDS->u_normalsTexture      = geometryData.currentNormals;
		reprojectVariableRateTextureDS->u_historyDepthTexture = geometryData.historyDepth;

		shaderConstants.reprojectionMaxPlaneDistance = geometryData.reprojectionMaxPlaneDistance;
	}

	reprojectVariableRateTextureDS->u_constants = shaderConstants;

	const rdr::PipelineStateID reprojectVariableRateTexturePipeline = CreateReprojectVariableRateTexturePipeline(vrSettings.permutationSettings, outputReprojectionSuccessMask, useDepthTest);

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{} - Reproject Variable Rate Texture", vrSettings.debugName.AsString()),
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

void VariableRateRenderer::Reinitialize(const VariableRateSettings& vrSettings)
{
	SPT_PROFILER_FUNCTION();

	Initialize(vrSettings);
}

void VariableRateRenderer::Reproject(rg::RenderGraphBuilder& graphBuilder, const VariableRateReprojectionParams& reprojectionParams)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(reprojectionParams.motionTexture.IsValid());

	const Bool canPerformReprojection = PrepareTextures(graphBuilder, reprojectionParams.motionTexture);

	const rg::RGTextureViewHandle reprojectionTarget = graphBuilder.AcquireExternalTextureView(m_reprojectionTargetVRTexture);

	if (canPerformReprojection)
	{
		const rg::RGTextureViewHandle reprojectionSource = graphBuilder.AcquireExternalTextureView(m_reprojectionSourceVRTexture);
		ReprojectVariableRateTexture(graphBuilder, m_vrSettings, reprojectionSource, reprojectionTarget, reprojectionParams);
	}
	else
	{
		graphBuilder.ClearTexture(RG_DEBUG_NAME("Clear Variable Rate Texture"), reprojectionTarget, rhi::ClearColor(0u, 0u, 0u, 0u));
	}
}

void VariableRateRenderer::Render(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle inputTexture, std::optional<rdr::ShaderID> customShader /*= {}*/, lib::Span<const lib::MTHandle<rg::RGDescriptorSetStateBase>> additionalDescriptorSets /*= {}*/)
{
	SPT_PROFILER_FUNCTION();

	std::swap(m_reprojectionSourceVRTexture, m_reprojectionTargetVRTexture);

	const rg::RGTextureViewHandle variableRateTexture = graphBuilder.AcquireExternalTextureView(m_reprojectionSourceVRTexture);

	RenderVariableRateTexture(graphBuilder, m_vrSettings, inputTexture, m_resolution, variableRateTexture, m_frameIdx, customShader, additionalDescriptorSets);

	m_frameIdx++;
}

Bool VariableRateRenderer::PrepareTextures(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle inputTexture)
{
	m_resolution = inputTexture->GetResolution2D();

	const math::Vector2u variableRateTextureResolution = ComputeVariableRateTextureResolution(inputTexture->GetResolution2D(), GetVariableRateSettings());

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
