#include "HDRResolveRenderStage.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "Common/ShaderCompilationInput.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "View/ViewRenderingSpec.h"
#include "View/RenderView.h"
#include "Engine.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Paramters =====================================================================================

namespace params
{

RendererFloatParameter adaptationSpeed("Adaptation Speed", { "Exposure" }, 1.f, 0.f, 20.f);
RendererFloatParameter minLogLuminance("Min Log Luminance", { "Exposure" }, -10.f, -20.f, 20.f);
RendererFloatParameter maxLogLuminance("Max Log Luminance", { "Exposure" }, 2.f, -20.f, 20.f);

} // params

//////////////////////////////////////////////////////////////////////////////////////////////////
// Exposure ======================================================================================

namespace exposure
{

BEGIN_SHADER_STRUCT(ExposureSettings)
	SHADER_STRUCT_FIELD(math::Vector2u, textureSize)
	SHADER_STRUCT_FIELD(math::Vector2f, invTextureSize)
	SHADER_STRUCT_FIELD(Real32, deltaTime)
	SHADER_STRUCT_FIELD(Real32, minLogLuminance)
	SHADER_STRUCT_FIELD(Real32, maxLogLuminance)
	SHADER_STRUCT_FIELD(Real32, logLuminanceRange)
	SHADER_STRUCT_FIELD(Real32, inverseLogLuminanceRange)
	SHADER_STRUCT_FIELD(Real32, adaptationSpeed)
END_SHADER_STRUCT();


DS_BEGIN(LuminanceHistogramDS, rg::RGDescriptorSetState<LuminanceHistogramDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),								u_radianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_sampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<ExposureSettings>),					u_exposureSettings)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),								u_luminanceHistogram)
DS_END();


static rdr::PipelineStateID CompileLuminanceHistogramPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "LuminanceHistogramCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/CreateLuminanceHistogram.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("LuminanceHistogramPipeline"), shader);
}

static rg::RGBufferViewHandle CreateLuminanceHistogram(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const ExposureSettings& exposureSettings, rg::RGTextureViewHandle radianceTexture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(radianceTexture.IsValid());

	const Uint64 histogramBinsNum = 256;

	const Uint64 histogramSize = sizeof(Uint32) * histogramBinsNum;
	const rhi::BufferDefinition luminanceHistogramDef(histogramSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
	const rg::RGBufferViewHandle luminanceHistogramBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("LuminanceHistogram"), luminanceHistogramDef, rhi::EMemoryUsage::GPUOnly);

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Reset Luminance Histogram"), luminanceHistogramBuffer, 0, histogramSize, 0);
	
	const lib::SharedRef<LuminanceHistogramDS> luminanceHistogramDS = rdr::ResourcesManager::CreateDescriptorSetState<LuminanceHistogramDS>(RENDERER_RESOURCE_NAME("LuminanceHistogramDS"));
	luminanceHistogramDS->u_radianceTexture		= radianceTexture;
	luminanceHistogramDS->u_exposureSettings	= exposureSettings;
	luminanceHistogramDS->u_luminanceHistogram	= luminanceHistogramBuffer;

	static const rdr::PipelineStateID pipelineState = CompileLuminanceHistogramPipeline();

	const math::Vector2u textureRes = exposureSettings.textureSize;
	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(textureRes.x(), 16u), math::Utils::DivideCeil(textureRes.y(), 16u), 1);
	graphBuilder.Dispatch(RG_DEBUG_NAME("Luminance Histogram"), pipelineState, dispatchGroupsNum, rg::BindDescriptorSets(luminanceHistogramDS));

	return luminanceHistogramBuffer;
}


DS_BEGIN(ComputeAdaptedLuminanceDS, rg::RGDescriptorSetState<ComputeAdaptedLuminanceDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<ExposureSettings>),	u_exposureSettings)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),					u_luminanceHistogram)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Real32>),				u_adaptedLuminance)
DS_END();


struct ViewLuminanceData
{
	lib::SharedPtr<rdr::Buffer> adaptedLuminance;
};

static rdr::PipelineStateID CompileAdaptedLuminancePipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "AdaptedLuminanceCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/ComputeAdaptedLuminance.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("AdaptedLuminancePipeline"), shader);
}

rg::RGBufferViewHandle ComputeAdaptedLuminance(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const ExposureSettings& exposureSettings, rg::RGBufferViewHandle luminanceHistogram)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(luminanceHistogram.IsValid());

	const RenderSceneEntityHandle& viewEntity = viewSpec.GetRenderView().GetViewEntity();
	ViewLuminanceData& viewLuminanceData = viewEntity.get_or_emplace<ViewLuminanceData>();
	
	if (!viewLuminanceData.adaptedLuminance)
	{
		const Uint64 adaptedLuminanceBufferSize = sizeof(Real32);
		rhi::BufferDefinition adaptedLuminanceDef(adaptedLuminanceBufferSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
		viewLuminanceData.adaptedLuminance = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("AdaptedLuminanceBuffer"), adaptedLuminanceDef, rhi::EMemoryUsage::GPUOnly);
	}

	const rg::RGBufferViewHandle adaptedLuminanceBuffer = graphBuilder.AcquireExternalBufferView(viewLuminanceData.adaptedLuminance->CreateFullView());

	const lib::SharedRef<ComputeAdaptedLuminanceDS> computeAdaptedLuminanceDS = rdr::ResourcesManager::CreateDescriptorSetState<ComputeAdaptedLuminanceDS>(RENDERER_RESOURCE_NAME("ComputeAdaptedLuminanceDS"));
	computeAdaptedLuminanceDS->u_exposureSettings	= exposureSettings;
	computeAdaptedLuminanceDS->u_luminanceHistogram = luminanceHistogram;
	computeAdaptedLuminanceDS->u_adaptedLuminance	= adaptedLuminanceBuffer;

	static const rdr::PipelineStateID pipelineState = CompileAdaptedLuminancePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Adapted Luminance"), pipelineState, math::Vector3u(1, 1, 1), rg::BindDescriptorSets(computeAdaptedLuminanceDS));

	return adaptedLuminanceBuffer;
}

} // exposure

//////////////////////////////////////////////////////////////////////////////////////////////////
// Tonemapping ===================================================================================

namespace tonemapping
{

BEGIN_SHADER_STRUCT(TonemappingSettings)
	SHADER_STRUCT_FIELD(math::Vector2u, textureSize)
	SHADER_STRUCT_FIELD(math::Vector2f, invTextureSize)
END_SHADER_STRUCT();


DS_BEGIN(TonemappingDS, rg::RGDescriptorSetState<TonemappingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),								u_radianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_sampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),								u_LDRTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TonemappingSettings>),						u_tonemappingSettings)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Real32>),									u_adaptedLuminance)
DS_END();


static rdr::PipelineStateID CompileTonemappingPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TonemappingCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Tonemapping.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TonemappingPipeline"), shader);
}

static void DoTonemappingAndGammaCorrection(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle radianceTexture, const TonemappingSettings& tonemappingSettings, rg::RGTextureViewHandle ldrTexture, rg::RGBufferViewHandle adaptedLuminance)
{
	SPT_PROFILER_FUNCTION();
	
	const lib::SharedRef<TonemappingDS> tonemappingDS = rdr::ResourcesManager::CreateDescriptorSetState<TonemappingDS>(RENDERER_RESOURCE_NAME("TonemappingDS"));
	tonemappingDS->u_radianceTexture		= radianceTexture;
	tonemappingDS->u_LDRTexture				= ldrTexture;
	tonemappingDS->u_tonemappingSettings	= tonemappingSettings;
	tonemappingDS->u_adaptedLuminance		= adaptedLuminance;

	static const rdr::PipelineStateID pipelineState = CompileTonemappingPipeline();

	const math::Vector2u textureRes = tonemappingSettings.textureSize;
	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(textureRes.x(), 8u), math::Utils::DivideCeil(textureRes.y(), 8u), 1);
	graphBuilder.Dispatch(RG_DEBUG_NAME("Tonemapping And Gamma"), pipelineState, dispatchGroupsNum, rg::BindDescriptorSets(tonemappingDS));
}

} // tonemapping

//////////////////////////////////////////////////////////////////////////////////////////////////
// HDRResolveRenderStage =========================================================================

HDRResolveRenderStage::HDRResolveRenderStage()
{ }

void HDRResolveRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	const ShadingData& shadingData	= viewSpec.GetData().Get<ShadingData>();
	HDRResolvePassData& passData	= viewSpec.GetData().Create<HDRResolvePassData>();
	
	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();
	const math::Vector3u textureRes(renderingRes.x(), renderingRes.y(), 1);
	const math::Vector2f invTextureRes = math::Vector2f(1.f / static_cast<Real32>(renderingRes.x()), 1.f / static_cast<Real32>(renderingRes.y()));
	
	rhi::TextureDefinition tonemappedTextureDef;
	tonemappedTextureDef.resolution = textureRes;
	tonemappedTextureDef.usage		= lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferSource);
	tonemappedTextureDef.format		= rhi::EFragmentFormat::RGBA8_UN_Float;
	passData.tonemappedTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("TonemappedTexture"), tonemappedTextureDef, rhi::EMemoryUsage::GPUOnly);

	exposure::ExposureSettings exposureSettings;
	exposureSettings.textureSize				= renderingRes;
	exposureSettings.invTextureSize				= invTextureRes;
	exposureSettings.deltaTime					= engn::Engine::Get().GetEngineTimer().GetDeltaTime();
	exposureSettings.minLogLuminance			= params::minLogLuminance;
	exposureSettings.maxLogLuminance			= params::maxLogLuminance;
	exposureSettings.logLuminanceRange			= exposureSettings.maxLogLuminance - exposureSettings.minLogLuminance;
	exposureSettings.inverseLogLuminanceRange	= 1.f / exposureSettings.logLuminanceRange;
	exposureSettings.adaptationSpeed			= params::adaptationSpeed;

	const rg::RGBufferViewHandle luminanceHistogram = exposure::CreateLuminanceHistogram(graphBuilder, viewSpec, exposureSettings, shadingData.radiance);

	const rg::RGBufferViewHandle adaptedLuminance = exposure::ComputeAdaptedLuminance(graphBuilder, viewSpec, exposureSettings, luminanceHistogram);

	tonemapping::TonemappingSettings tonemappingSettings;
	tonemappingSettings.textureSize		= renderingRes;
	tonemappingSettings.invTextureSize	= invTextureRes;

	tonemapping::DoTonemappingAndGammaCorrection(graphBuilder, viewSpec, shadingData.radiance, tonemappingSettings, passData.tonemappedTexture, adaptedLuminance);
	
	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
}

} // spt::rsc
