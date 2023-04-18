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
#include "Transfers/UploadUtils.h"
#include "EngineFrame.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Paramters =====================================================================================

namespace params
{

RendererFloatParameter adaptationSpeed("Adaptation Speed", { "Exposure" }, 0.4f, 0.f, 1.f);
RendererFloatParameter minLogLuminance("Min Log Luminance", { "Exposure" }, -10.f, -20.f, 20.f);
RendererFloatParameter maxLogLuminance("Max Log Luminance", { "Exposure" }, 2.f, -20.f, 20.f);

RendererBoolParameter enableBloom("Enable Bloom", { "Bloom" }, true);
RendererFloatParameter bloomEC("BloomEC", { "Bloom" }, 2.1f, -10.f, 10.f);
RendererFloatParameter bloomScale("Bloom Scale", { "Bloom" }, 1.f, 0.f, 5.f);
RendererFloatParameter bloomThreshold("Bloom Threshold", { "Bloom" }, 0.95f, 0.f, 1.f);

RendererBoolParameter enableColorDithering("Enable Color Dithering", { "Post Process" }, true);

} // params

//////////////////////////////////////////////////////////////////////////////////////////////////
// Exposure ======================================================================================

namespace exposure
{

BEGIN_SHADER_STRUCT(ExposureSettings)
	SHADER_STRUCT_FIELD(math::Vector2u, textureSize)
	SHADER_STRUCT_FIELD(math::Vector2f, inputPixelSize)
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
		constexpr Real32 defaultAadptedLuminance = 0.18f;
		gfx::FillBuffer(lib::Ref(viewLuminanceData.adaptedLuminance), 0, sizeof(Real32), *reinterpret_cast<const Uint32*>(&defaultAadptedLuminance));
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
// Bloom =========================================================================================

namespace bloom
{

BEGIN_SHADER_STRUCT(BloomPassInfo)
	SHADER_STRUCT_FIELD(math::Vector2f, inputPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, outputPixelSize)
	SHADER_STRUCT_FIELD(Bool, prefilterInput)
	SHADER_STRUCT_FIELD(Real32, bloomEC)
	SHADER_STRUCT_FIELD(Real32, bloomThreshold)
END_SHADER_STRUCT();


DS_BEGIN(BloomPassDS, rg::RGDescriptorSetState<BloomPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<BloomPassInfo>),					u_bloomInfo)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),								u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_inputSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),								u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalStructuredBufferBinding<Real32>),							u_adaptedLuminance)
DS_END();


static rdr::PipelineStateID CompileBloomDownsamplePipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BloomDownsampleCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Bloom.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BloomDownsamplePipeline"), shader);
}


static rdr::PipelineStateID CompileBloomUpsamplePipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BloomUpsampleCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Bloom.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BloomUpsamplePipeline"), shader);
}

static void BloomDownsample(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGBufferViewHandle adaptedLuminance, const lib::DynamicArray<rg::RGTextureViewHandle>& bloomTextureMips, rg::RGTextureViewHandle inputTexture)
{
	SPT_PROFILER_FUNCTION();
	
	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();

	const Int32 bloomPassesNum = static_cast<Int32>(bloomTextureMips.size());

	static const rdr::PipelineStateID downsamplePipeline = CompileBloomDownsamplePipeline();

	rg::RGTextureViewHandle inputTextureView = inputTexture;

	for (Int32 passIdx = 0; passIdx < bloomPassesNum; ++passIdx)
	{
		const math::Vector2u inputRes = math::Vector2u(renderingRes.x() >> passIdx, renderingRes.y() >> passIdx);
		const math::Vector2u outputRes = math::Vector2u(renderingRes.x() >> (passIdx + 1), renderingRes.y() >> (passIdx + 1));

		rg::RGTextureViewHandle outputTextureView = bloomTextureMips[passIdx];

		BloomPassInfo passInfo;
		passInfo.inputPixelSize		= inputRes.cast<Real32>().cwiseInverse();
		passInfo.outputPixelSize	= outputRes.cast<Real32>().cwiseInverse();
		passInfo.prefilterInput		= passIdx == 0;
		passInfo.bloomEC			= params::bloomEC;
		passInfo.bloomThreshold		= params::bloomThreshold;

		const lib::SharedRef<BloomPassDS> bloomPassDS = rdr::ResourcesManager::CreateDescriptorSetState<BloomPassDS>(RENDERER_RESOURCE_NAME(std::format("BloomDownsampleDS(%d)", passIdx)));
		bloomPassDS->u_bloomInfo		= passInfo;
		bloomPassDS->u_inputTexture		= inputTextureView;
		bloomPassDS->u_outputTexture	= outputTextureView;
		if (passInfo.prefilterInput)
		{
			bloomPassDS->u_adaptedLuminance = adaptedLuminance;
		}

		const math::Vector3u groupCount(math::Utils::DivideCeil(outputRes.x(), 8u), math::Utils::DivideCeil(outputRes.y(), 8u), 1u);
		graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("Bloom Downsample [{}, {}] -> [{}, {}]", inputRes.x(), inputRes.y(), outputRes.x(), outputRes.y())),
							  downsamplePipeline, 
							  groupCount,
							  rg::BindDescriptorSets(bloomPassDS));

		inputTextureView = outputTextureView;
	}
}

static void BloomUpsample(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const lib::DynamicArray<rg::RGTextureViewHandle>& bloomTextureMips)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();

	const Int32 bloomPassesNum = static_cast<Int32>(bloomTextureMips.size());

	static const rdr::PipelineStateID upsamplePipeline = CompileBloomUpsamplePipeline();

	rg::RGTextureViewHandle inputTextureView = bloomTextureMips.back();

	// Upsample passes
	for (Int32 passIdx = static_cast<Int32>(bloomPassesNum) - 2; passIdx >= 0; --passIdx)
	{
		const math::Vector2u inputRes = math::Vector2u(renderingRes.x() >> (passIdx + 2), renderingRes.y() >> (passIdx + 2));
		const math::Vector2u outputRes = math::Vector2u(renderingRes.x() >> (passIdx + 1), renderingRes.y() >> (passIdx + 1));

		rg::RGTextureViewHandle outputTextureView = bloomTextureMips[passIdx];

		BloomPassInfo passInfo;
		passInfo.inputPixelSize		= inputRes.cast<Real32>().cwiseInverse() * params::bloomScale;
		passInfo.outputPixelSize	= outputRes.cast<Real32>().cwiseInverse();
		passInfo.prefilterInput		= false;

		const lib::SharedRef<BloomPassDS> bloomPassDS = rdr::ResourcesManager::CreateDescriptorSetState<BloomPassDS>(RENDERER_RESOURCE_NAME(std::format("BloomUpsampleDS(%d)", passIdx)));
		bloomPassDS->u_bloomInfo		= passInfo;
		bloomPassDS->u_inputTexture		= inputTextureView;
		bloomPassDS->u_outputTexture	= outputTextureView;

		const math::Vector3u groupCount(math::Utils::DivideCeil(outputRes.x(), 8u), math::Utils::DivideCeil(outputRes.y(), 8u), 1u);
		graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("Bloom Upsample [{}, {}] -> [{}, {}]", inputRes.x(), inputRes.y(), outputRes.x(), outputRes.y())),
							  upsamplePipeline, 
							  groupCount,
							  rg::BindDescriptorSets(bloomPassDS));

		inputTextureView = outputTextureView;
	}
}

static void BloomComposite(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGBufferViewHandle adaptedLuminance, rg::RGTextureViewHandle bloomTextureMip0, rg::RGTextureViewHandle outputTexture)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();

	static const rdr::PipelineStateID upsamplePipeline = CompileBloomUpsamplePipeline();

	BloomPassInfo passInfo;
	passInfo.inputPixelSize			= math::Vector2f(1.f / static_cast<Real32>(renderingRes.x() >> 1), 1.f / static_cast<Real32>(renderingRes.y() >> 1)) * params::bloomScale;
	passInfo.outputPixelSize		= math::Vector2f(1.f / static_cast<Real32>(renderingRes.x()), 1.f / static_cast<Real32>(renderingRes.y()));

	const lib::SharedRef<BloomPassDS> bloomPassDS = rdr::ResourcesManager::CreateDescriptorSetState<BloomPassDS>(RENDERER_RESOURCE_NAME("BloomCompositeDS"));
	bloomPassDS->u_bloomInfo		= passInfo;
	bloomPassDS->u_inputTexture		= bloomTextureMip0;
	bloomPassDS->u_outputTexture	= outputTexture;
	bloomPassDS->u_adaptedLuminance = adaptedLuminance;

	const math::Vector3u groupCount(math::Utils::DivideCeil(renderingRes.x(), 8u), math::Utils::DivideCeil(renderingRes.y(), 8u), 1u);
	graphBuilder.Dispatch(RG_DEBUG_NAME("Bloom Composite"),
						  upsamplePipeline, 
						  groupCount,
						  rg::BindDescriptorSets(bloomPassDS));
}

static void ApplyBloom(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGBufferViewHandle adaptedLuminance, rg::RGTextureViewHandle radianceTexture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(adaptedLuminance.IsValid())

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();
	const Uint32 bloomPassesNum = std::max(math::Utils::ComputeMipLevelsNumForResolution(renderingRes), 6u) - 5u;

	rhi::TextureDefinition bloomTextureDef;
	bloomTextureDef.resolution	= math::Vector3u(renderingRes.x() / 2, renderingRes.y() / 2, 1);
	bloomTextureDef.usage		= lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);
	bloomTextureDef.format		= rhi::EFragmentFormat::B10G11B11_U_Float;
	bloomTextureDef.samples		= 1;
	bloomTextureDef.mipLevels	= bloomPassesNum;
	bloomTextureDef.arrayLayers	= 1;
	const rg::RGTextureHandle bloomTexture = graphBuilder.CreateTexture(RG_DEBUG_NAME("Bloom Downsample Texture"), bloomTextureDef, rhi::EMemoryUsage::GPUOnly);

	lib::DynamicArray<rg::RGTextureViewHandle> bloomTextureMips;
	bloomTextureMips.reserve(static_cast<SizeType>(bloomPassesNum));
	for (SizeType mipIdx = 0; mipIdx < bloomPassesNum; ++mipIdx)
	{
		rhi::TextureViewDefinition viewInfo;
		viewInfo.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color, static_cast<Uint32>(mipIdx), 1);

		bloomTextureMips.emplace_back(graphBuilder.CreateTextureView(RG_DEBUG_NAME(std::format("Bloom Texture Mip {}", mipIdx)), bloomTexture, viewInfo));
	}

	BloomDownsample(graphBuilder, viewSpec, adaptedLuminance, bloomTextureMips, radianceTexture);

	BloomUpsample(graphBuilder, viewSpec, bloomTextureMips);

	BloomComposite(graphBuilder, viewSpec, adaptedLuminance, bloomTextureMips[0], radianceTexture);
}

} // bloom

//////////////////////////////////////////////////////////////////////////////////////////////////
// Tonemapping ===================================================================================

namespace tonemapping
{

BEGIN_SHADER_STRUCT(TonemappingPassSettings)
	SHADER_STRUCT_FIELD(Bool, enableColorDithering)
END_SHADER_STRUCT();


DS_BEGIN(TonemappingDS, rg::RGDescriptorSetState<TonemappingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),								u_radianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_sampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),								u_LDRTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TonemappingPassSettings>),					u_tonemappingSettings)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Real32>),									u_adaptedLuminance)
DS_END();


static rdr::PipelineStateID CompileTonemappingPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TonemappingCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Tonemapping.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TonemappingPipeline"), shader);
}

static void DoTonemappingAndGammaCorrection(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle radianceTexture, const TonemappingPassSettings& tonemappingSettings, rg::RGTextureViewHandle ldrTexture, rg::RGBufferViewHandle adaptedLuminance)
{
	SPT_PROFILER_FUNCTION();
	
	const lib::SharedRef<TonemappingDS> tonemappingDS = rdr::ResourcesManager::CreateDescriptorSetState<TonemappingDS>(RENDERER_RESOURCE_NAME("TonemappingDS"));
	tonemappingDS->u_radianceTexture		= radianceTexture;
	tonemappingDS->u_LDRTexture				= ldrTexture;
	tonemappingDS->u_tonemappingSettings	= tonemappingSettings;
	tonemappingDS->u_adaptedLuminance		= adaptedLuminance;

	static const rdr::PipelineStateID pipelineState = CompileTonemappingPipeline();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingResolution();

	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(renderingRes.x(), 8u), math::Utils::DivideCeil(renderingRes.y(), 8u), 1u);
	graphBuilder.Dispatch(RG_DEBUG_NAME("Tonemapping And Gamma"), pipelineState, dispatchGroupsNum, rg::BindDescriptorSets(tonemappingDS));
}

} // tonemapping

//////////////////////////////////////////////////////////////////////////////////////////////////
// gamma =========================================================================================

namespace gamma
{

DS_BEGIN(GammaCorrectionDS, rg::RGDescriptorSetState<GammaCorrectionDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>), u_texture)
DS_END();


rdr::PipelineStateID CompileGammaCorrectionPipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddShaderToCompile(sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GammaCorrectionCS"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/GammaCorrection.hlsl", compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("GammaCorrectionPipeline"), shader);
}

void DoGammaCorrection(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle texture)
{
	SPT_PROFILER_FUNCTION();
	
	const lib::SharedRef<GammaCorrectionDS> gammaCorrectionCS = rdr::ResourcesManager::CreateDescriptorSetState<GammaCorrectionDS>(RENDERER_RESOURCE_NAME("GammaCorrectionDS"));
	gammaCorrectionCS->u_texture = texture;

	static const rdr::PipelineStateID pipelineState = CompileGammaCorrectionPipeline();

	const math::Vector2u textureRes = texture->GetResolution2D();
	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(textureRes.x(), 8u), math::Utils::DivideCeil(textureRes.y(), 8u), 1);
	graphBuilder.Dispatch(RG_DEBUG_NAME("Gamma Correction"), pipelineState, dispatchGroupsNum, rg::BindDescriptorSets(gammaCorrectionCS));
}

} // gamma

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
	const math::Vector2f inputPixelSize = math::Vector2f(1.f / static_cast<Real32>(renderingRes.x()), 1.f / static_cast<Real32>(renderingRes.y()));
	
	rhi::TextureDefinition tonemappedTextureDef;
	tonemappedTextureDef.resolution = textureRes;
	tonemappedTextureDef.usage		= lib::Flags(rhi::ETextureUsage::ColorRT, rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::TransferSource);
	tonemappedTextureDef.format		= rhi::EFragmentFormat::RGBA8_UN_Float;
	passData.tonemappedTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("TonemappedTexture"), tonemappedTextureDef, rhi::EMemoryUsage::GPUOnly);

	exposure::ExposureSettings exposureSettings;
	exposureSettings.textureSize				= renderingRes;
	exposureSettings.inputPixelSize				= inputPixelSize;
	exposureSettings.deltaTime					= engn::GetRenderingFrame().GetDeltaTime();
	exposureSettings.minLogLuminance			= params::minLogLuminance;
	exposureSettings.maxLogLuminance			= params::maxLogLuminance;
	exposureSettings.logLuminanceRange			= exposureSettings.maxLogLuminance - exposureSettings.minLogLuminance;
	exposureSettings.inverseLogLuminanceRange	= 1.f / exposureSettings.logLuminanceRange;
	exposureSettings.adaptationSpeed			= params::adaptationSpeed;

	const rg::RGBufferViewHandle luminanceHistogram = exposure::CreateLuminanceHistogram(graphBuilder, viewSpec, exposureSettings, shadingData.radiance);
	const rg::RGBufferViewHandle adaptedLuminance = exposure::ComputeAdaptedLuminance(graphBuilder, viewSpec, exposureSettings, luminanceHistogram);

	if (params::enableBloom)
	{
		bloom::ApplyBloom(graphBuilder, viewSpec, adaptedLuminance, shadingData.radiance);
	}

	tonemapping::TonemappingPassSettings tonemappingSettings;
	tonemappingSettings.enableColorDithering = params::enableColorDithering;

	tonemapping::DoTonemappingAndGammaCorrection(graphBuilder, viewSpec, shadingData.radiance, tonemappingSettings, passData.tonemappedTexture, adaptedLuminance);

#if RENDERER_DEBUG
	gamma::DoGammaCorrection(graphBuilder, shadingData.debug);
	passData.debug = shadingData.debug;
#endif // RENDERER_DEBUG
	
	GetStageEntries(viewSpec).GetOnRenderStage().Broadcast(graphBuilder, renderScene, viewSpec, stageContext);
	
	RenderViewEntryContext context;
	context.texture = passData.tonemappedTexture;
	viewSpec.GetRenderViewEntry(RenderViewEntryDelegates::RenderSceneDebugLayer).Broadcast(graphBuilder, renderScene, viewSpec, context);
}

} // spt::rsc
