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
#include "View/ViewRenderingSpec.h"
#include "View/RenderView.h"
#include "Engine.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "Transfers/UploadUtils.h"
#include "EngineFrame.h"
#include "Camera/CameraSettings.h"
#include "RenderScene.h"
#include "SceneRenderer/RenderStages/Utils/BilateralGridRenderer.h"
#include "SceneRenderer/Utils/GaussianBlurRenderer.h"

namespace spt::rsc
{

REGISTER_RENDER_STAGE(ERenderStage::HDRResolve, HDRResolveRenderStage);

//////////////////////////////////////////////////////////////////////////////////////////////////
// Paramters =====================================================================================

namespace params
{

RendererFloatParameter adaptationSpeed("Adaptation Speed", { "Exposure" }, 1.65f, 0.f, 8.f);
RendererFloatParameter minLogLuminance("Min Log Luminance", { "Exposure" }, -10.f, -20.f, 20.f);
RendererFloatParameter maxLogLuminance("Max Log Luminance", { "Exposure" }, 20.f, -20.f, 20.f);
RendererFloatParameter rejectedDarkPixelsPercentage("Rejected Dark Pixels Percentage", { "Exposure" }, 0.6f, 0.f, 0.8f);
RendererFloatParameter rejectedBrightPixelsPercentage("Rejected Bright Pixels Percentage", { "Exposure" }, 0.05f, 0.f, 0.1f);
RendererFloatParameter evCompensation("EV Compensation", { "Exposure" }, -1.f, -10.f, 10.f);

RendererBoolParameter enableBloom("Enable Bloom", { "Bloom" }, true);
RendererFloatParameter bloomIntensity("Bloom Intensity", { "Bloom" }, 1.0f, 0.f, 10.f);
RendererFloatParameter bloomBlendFactor("Bloom Blend Factor", { "Bloom" }, 0.35f, 0.f, 1.f);
RendererFloatParameter bloomUpsampleBlendFactor("Bloom Upsample Blend Factor", { "Bloom" }, 0.45f, 0.f, 1.f);

RendererBoolParameter enableLensFlares("Enable Lens Flares", { "Lens Flares" }, true);
RendererFloatParameter lensFlaresIntensity("Lens Flares Intensity", { "Lens Flares" }, 0.006f, 0.f, 1.f);
RendererFloat3Parameter lensFlaresLinearColorThreshold("Lens Flares Threshold", { "Lens Flares" }, math::Vector3f(360.f, 360.f, 220.f), 0.f, 1000.f);

RendererIntParameter lensFlaresGhostsNum("Lens Flares GhostsNum", { "Lens Flares", "Ghosts" }, 6, 0, 10);
RendererFloatParameter lensFlaresGhostsDispersal("Lens Flares Ghosts Dispersal", { "Lens Flares", "Ghosts" }, 0.3f, 0.f, 1.f);
RendererFloatParameter lensFlaresGhostsIntensity("Lens Flares Ghosts Intensity", { "Lens Flares", "Ghosts" }, 1.0f, 0.f, 10.f);
RendererFloat3Parameter lensFlaresGhostsDistortion("Lens Flares Ghosts Distortion", { "Lens Flares", "Ghosts" }, math::Vector3f(0.f, 0.03f, 0.076f), 0.f, 1.f);

RendererFloatParameter lensFlaresHaloIntensity("Lens Flares Halo Intensity", { "Lens Flares", "Halo" }, 1.0f, 0.f, 3.f);
RendererFloat3Parameter lensFlaresHaloDistortion("Lens Flares Halo Distortion", { "Lens Flares", "Halo" }, math::Vector3f(0.f, 0.03f, 0.076f), 0.f, 1.f);
RendererFloatParameter lensFlaresHaloWidth("Lens Flares Halo Width", { "Lens Flares", "Halo" }, 0.42f, 0.f, 1.f);

RendererBoolParameter enableColorDithering("Enable Color Dithering", { "Post Process" }, true);

RendererIntParameter bilateralGridBlurXKernel("Blur X Kernel", { "LocalTonemap", "BilateralGrid" }, 12, 0, 32);
RendererIntParameter bilateralGridBlurYKernel("Blur Y Kernel", { "LocalTonemap", "BilateralGrid" }, 12, 0, 32);
RendererIntParameter bilateralGridBlurZKernel("Blur Z Kernel", { "LocalTonemap", "BilateralGrid" }, 12, 0, 32);
RendererFloatParameter bilateralGrid2DSigma("Blur 2D Sigma", { "LocalTonemap", "BilateralGrid" }, 8.f, 0.f, 32.f);
RendererFloatParameter bilateralGridDepthSigma("Blur Depth Sigma", { "LocalTonemap", "BilateralGrid" }, 2.2f, 0.f, 32.f);

RendererIntParameter logLuminanceBlurXKernel("Blur X Kernel", { "LocalTonemap", "LogLuminance" }, 14, 0, 32);
RendererIntParameter logLuminanceBlurYKernel("Blur Y Kernel", { "LocalTonemap", "LogLuminance" }, 14, 0, 32);
RendererFloatParameter logLuminanceBlurSigma("Blur 2D Sigma", { "LocalTonemap", "LogLuminance" }, 8.f, 0.f, 32.f);

RendererFloatParameter detailStrength("Detail Strength", { "LocalTonemap" }, 0.9f, 0.f, 2.f);
RendererFloatParameter contrastStrength("Contrast Strength", { "LocalTonemap" }, 0.7f, 0.f, 2.f);

RendererFloatParameter bilateralGridStrength("Bilateral Grid Strength", { "LocalTonemap" }, 0.4f, 0.f, 1.f);

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
	SHADER_STRUCT_FIELD(Real32, rejectedDarkPixelsPercentage)
	SHADER_STRUCT_FIELD(Real32, rejectedBrightPixelsPercentage)
	SHADER_STRUCT_FIELD(Real32, evCompensation)
END_SHADER_STRUCT();


DS_BEGIN(LuminanceHistogramDS, rg::RGDescriptorSetState<LuminanceHistogramDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                            u_linearColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>), u_sampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<ExposureSettings>),                        u_exposureSettings)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Uint32>),                              u_luminanceHistogram)
DS_END();


static rdr::PipelineStateID CompileLuminanceHistogramPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/CreateLuminanceHistogram.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "LuminanceHistogramCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("LuminanceHistogramPipeline"), shader);
}


static rg::RGBufferViewHandle CreateLuminanceHistogram(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const ExposureSettings& exposureSettings, rg::RGTextureViewHandle linearColorTexture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(linearColorTexture.IsValid());

	const Uint64 histogramBinsNum = 256;

	const Uint64 histogramSize = sizeof(Uint32) * histogramBinsNum;
	const rhi::BufferDefinition luminanceHistogramDef(histogramSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
	const rg::RGBufferViewHandle luminanceHistogramBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("LuminanceHistogram"), luminanceHistogramDef, rhi::EMemoryUsage::GPUOnly);

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Reset Luminance Histogram"), luminanceHistogramBuffer, 0, histogramSize, 0);
	
	const lib::MTHandle<LuminanceHistogramDS> luminanceHistogramDS = graphBuilder.CreateDescriptorSet<LuminanceHistogramDS>(RENDERER_RESOURCE_NAME("LuminanceHistogramDS"));
	luminanceHistogramDS->u_linearColorTexture	= linearColorTexture;
	luminanceHistogramDS->u_exposureSettings	= exposureSettings;
	luminanceHistogramDS->u_luminanceHistogram	= luminanceHistogramBuffer;

	static const rdr::PipelineStateID pipelineState = CompileLuminanceHistogramPipeline();

	const math::Vector2u textureRes = exposureSettings.textureSize;
	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(textureRes.x(), 16u), math::Utils::DivideCeil(textureRes.y(), 16u), 1);
	graphBuilder.Dispatch(RG_DEBUG_NAME("Luminance Histogram"),
						  pipelineState,
						  dispatchGroupsNum,
						  rg::BindDescriptorSets(luminanceHistogramDS,
												 viewSpec.GetRenderView().GetRenderViewDS()));

	return luminanceHistogramBuffer;
}


DS_BEGIN(ComputeAdaptedLuminanceDS, rg::RGDescriptorSetState<ComputeAdaptedLuminanceDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<ExposureSettings>),     u_exposureSettings)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<Uint32>),             u_luminanceHistogram)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<Real32>),           u_adaptedLuminance)
	DS_BINDING(BINDING_TYPE(gfx::RWStructuredBufferBinding<ViewExposureData>), u_viewExposureData)
DS_END();


struct ViewLuminanceData
{
	lib::SharedPtr<rdr::Buffer> adaptedLuminance;
};


static rdr::PipelineStateID CompileAdaptedLuminancePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/ComputeAdaptedLuminance.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "AdaptedLuminanceCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("AdaptedLuminancePipeline"), shader);
}


rg::RGBufferViewHandle ComputeAdaptedLuminance(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const ExposureSettings& exposureSettings, rg::RGBufferViewHandle luminanceHistogram, rg::RGBufferViewHandle viewExposureData)
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

	const lib::MTHandle<ComputeAdaptedLuminanceDS> computeAdaptedLuminanceDS = graphBuilder.CreateDescriptorSet<ComputeAdaptedLuminanceDS>(RENDERER_RESOURCE_NAME("ComputeAdaptedLuminanceDS"));
	computeAdaptedLuminanceDS->u_exposureSettings	= exposureSettings;
	computeAdaptedLuminanceDS->u_luminanceHistogram = luminanceHistogram;
	computeAdaptedLuminanceDS->u_adaptedLuminance	= adaptedLuminanceBuffer;
	computeAdaptedLuminanceDS->u_viewExposureData	= viewExposureData;

	static const rdr::PipelineStateID pipelineState = CompileAdaptedLuminancePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Adapted Luminance"), pipelineState, math::Vector3u(1, 1, 1), rg::BindDescriptorSets(computeAdaptedLuminanceDS));

	return adaptedLuminanceBuffer;
}

} // exposure

//////////////////////////////////////////////////////////////////////////////////////////////////
// Lens Flares ===================================================================================

namespace lens_flares
{

BEGIN_SHADER_STRUCT(LensFlaresParams)
	SHADER_STRUCT_FIELD(math::Vector3f, ghostsDistortion)
	SHADER_STRUCT_FIELD(Uint32,			ghostsNum)
	SHADER_STRUCT_FIELD(math::Vector3f,	haloDistortion)
	SHADER_STRUCT_FIELD(Real32,			ghostsInensity)
	SHADER_STRUCT_FIELD(math::Vector3f,	linearColorThreshold)
	SHADER_STRUCT_FIELD(Real32,			ghostsDispersal)
	SHADER_STRUCT_FIELD(Real32,			ghostsIntensity)
	SHADER_STRUCT_FIELD(Real32,			haloIntensity)
	SHADER_STRUCT_FIELD(Real32,			haloWidth)
END_SHADER_STRUCT();


DS_BEGIN(LensFlaresPassDS, rg::RGDescriptorSetState<LensFlaresPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                            u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LensFlaresParams>),                       u_lensFlaresParams)
DS_END();


BEGIN_SHADER_STRUCT(LensFlaresBlurParams)
	SHADER_STRUCT_FIELD(Uint32, isHorizontal)
END_SHADER_STRUCT();


DS_BEGIN(LensFlaresBlurDS, rg::RGDescriptorSetState<LensFlaresBlurDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_inputSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector3f>),                            u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<LensFlaresBlurParams>),                   u_blurParams)
DS_END();


static rdr::PipelineStateID CompileComputeLensFlaresPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/LensFlares.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "ComputeLensFlaresCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("ComputeLensFlaresPipeline"), shader);
}


static rdr::PipelineStateID CompileLensFlaresBlurPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/LensFlaresBlur.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "LensFlaresBlurCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("LensFlaresBlurPipeline"), shader);
}


static rg::RGTextureViewHandle ComputeLensFlares(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle bloomTexture)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u lensFlaresRes = bloomTexture->GetResolution();

	const rg::RGTextureViewHandle lensFlaresTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Lens Flares Texture"), rg::TextureDef(lensFlaresRes, SceneRendererStatics::hdrFormat));
	
	const rg::RGTextureViewHandle tempTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Lens Flares Texture Temp"), rg::TextureDef(lensFlaresRes, SceneRendererStatics::hdrFormat));

	static const rdr::PipelineStateID computeLensFlaresPipeline = CompileComputeLensFlaresPipeline();

	LensFlaresParams lensFlaresParams;
	lensFlaresParams.ghostsDistortion		= params::lensFlaresGhostsDistortion;
	lensFlaresParams.ghostsNum				= params::lensFlaresGhostsNum;
	lensFlaresParams.haloDistortion			= params::lensFlaresHaloDistortion;
	lensFlaresParams.ghostsInensity			= params::lensFlaresGhostsIntensity;
	lensFlaresParams.ghostsDispersal		= params::lensFlaresGhostsDispersal;
	lensFlaresParams.ghostsIntensity		= params::lensFlaresGhostsIntensity;
	lensFlaresParams.haloIntensity			= params::lensFlaresHaloIntensity;
	lensFlaresParams.haloWidth				= params::lensFlaresHaloWidth;
	lensFlaresParams.linearColorThreshold	= params::lensFlaresLinearColorThreshold;

	const lib::MTHandle<LensFlaresPassDS> lensFlaresPassDS = graphBuilder.CreateDescriptorSet<LensFlaresPassDS>(RENDERER_RESOURCE_NAME("LensFlaresPassDS"));
	lensFlaresPassDS->u_inputTexture     = bloomTexture;
	lensFlaresPassDS->u_outputTexture    = lensFlaresTexture;
	lensFlaresPassDS->u_lensFlaresParams = lensFlaresParams;
	
	graphBuilder.Dispatch(RG_DEBUG_NAME("Apply Lens Flares"),
						  computeLensFlaresPipeline,
						  math::Utils::DivideCeil(lensFlaresRes, math::Vector3u(8u, 8u, 1u)),
						  rg::BindDescriptorSets(lensFlaresPassDS));

	static const rdr::PipelineStateID lensFlaresBlurPipeline = CompileLensFlaresBlurPipeline();

	const lib::MTHandle<LensFlaresBlurDS> blurHorizontalPassDS = graphBuilder.CreateDescriptorSet<LensFlaresBlurDS>(RENDERER_RESOURCE_NAME("LensFlaresHorizontalBlurDS"));
	blurHorizontalPassDS->u_inputTexture	= lensFlaresTexture;
	blurHorizontalPassDS->u_outputTexture	= tempTexture;
	blurHorizontalPassDS->u_blurParams		= LensFlaresBlurParams{ 1u };

	graphBuilder.Dispatch(RG_DEBUG_NAME("Lens Flares Horizontal Blur"),
						  lensFlaresBlurPipeline,
						  math::Utils::DivideCeil(lensFlaresRes, math::Vector3u(256u, 1u, 1u)),
						  rg::BindDescriptorSets(blurHorizontalPassDS));

	const lib::MTHandle<LensFlaresBlurDS> blurVerticalPassDS = graphBuilder.CreateDescriptorSet<LensFlaresBlurDS>(RENDERER_RESOURCE_NAME("LensFlaresVericalBlurDS"));
	blurVerticalPassDS->u_inputTexture		= tempTexture;
	blurVerticalPassDS->u_outputTexture		= lensFlaresTexture;
	blurVerticalPassDS->u_blurParams		= LensFlaresBlurParams{ 0u };

	graphBuilder.Dispatch(RG_DEBUG_NAME("Lens Flares Vertical Blur"),
						  lensFlaresBlurPipeline,
						  math::Utils::DivideCeil(math::Vector3u(lensFlaresRes.y(), lensFlaresRes.x(), lensFlaresRes.z()), math::Vector3u(256u, 1u, 1u)),
						  rg::BindDescriptorSets(blurVerticalPassDS));

	return lensFlaresTexture;
}

} // lens_flares

//////////////////////////////////////////////////////////////////////////////////////////////////
// Bloom =========================================================================================

namespace bloom
{

BEGIN_SHADER_STRUCT(BloomPassInfo)
	SHADER_STRUCT_FIELD(math::Vector2f, inputPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, outputPixelSize)
	SHADER_STRUCT_FIELD(Real32,			bloomUpsampleBlendFactor)
	SHADER_STRUCT_FIELD(Real32,			bloomIntensity)
	SHADER_STRUCT_FIELD(Bool,			isSetupPass)
END_SHADER_STRUCT();


DS_BEGIN(BloomPassDS, rg::RGDescriptorSetState<BloomPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<BloomPassInfo>),                          u_bloomInfo)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),                           u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),                            u_outputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearSampler)
DS_END();


BEGIN_SHADER_STRUCT(BloomCompositePassInfo)
	SHADER_STRUCT_FIELD(math::Vector3f,	lensDirtThreshold)
	SHADER_STRUCT_FIELD(Bool,			hasLensDirtTexture)
	SHADER_STRUCT_FIELD(math::Vector3f,	lensDirtIntensity)
	SHADER_STRUCT_FIELD(Real32,			bloomBlendFactor)
	SHADER_STRUCT_FIELD(Real32,			lensFlaresIntensity)
	SHADER_STRUCT_FIELD(Bool,			hasLensFlaresTexture)
END_SHADER_STRUCT()


DS_BEGIN(BloomCompositePassDS, rg::RGDescriptorSetState<BloomCompositePassDS>)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<math::Vector4f>),   u_lensDirtTexture)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<math::Vector4f>),   u_lensFlaresTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<BloomCompositePassInfo>), u_bloomCompositeInfo)
DS_END();


BloomPassInfo CreateBloomPassInfo(math::Vector2u inputRes, math::Vector2u outputRes)
{
	BloomPassInfo passInfo;
	passInfo.inputPixelSize				= inputRes.cast<Real32>().cwiseInverse();
	passInfo.outputPixelSize			= outputRes.cast<Real32>().cwiseInverse();
	passInfo.bloomUpsampleBlendFactor	= params::bloomUpsampleBlendFactor;
	passInfo.bloomIntensity				= params::bloomIntensity;

	return passInfo;
}


static rdr::PipelineStateID CompileBloomDownsamplePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Bloom.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BloomDownsampleCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BloomDownsamplePipeline"), shader);
}


static rdr::PipelineStateID CompileBloomUpsamplePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Bloom.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BloomUpsampleCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BloomUpsamplePipeline"), shader);
}


static rdr::PipelineStateID CompileBloomCompositePipeline()
{
	sc::ShaderCompilationSettings compilationSettings;
	compilationSettings.AddMacroDefinition(sc::MacroDefinition("BLOOM_COMPOSITE", "1"));
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Bloom.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BloomCompositeCS"), compilationSettings);

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BloomUpsamplePipeline"), shader);
}


static void BloomDownsample(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const lib::DynamicArray<rg::RGTextureViewHandle>& bloomTextureMips, rg::RGTextureViewHandle inputTexture)
{
	SPT_PROFILER_FUNCTION();
	
	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingRes();

	const Int32 bloomPassesNum = static_cast<Int32>(bloomTextureMips.size());

	static const rdr::PipelineStateID downsamplePipeline = CompileBloomDownsamplePipeline();

	rg::RGTextureViewHandle inputTextureView = inputTexture;

	for (Int32 passIdx = 0; passIdx < bloomPassesNum; ++passIdx)
	{
		const math::Vector2u inputRes = math::Vector2u(renderingRes.x() >> passIdx, renderingRes.y() >> passIdx);
		const math::Vector2u outputRes = math::Vector2u(renderingRes.x() >> (passIdx + 1), renderingRes.y() >> (passIdx + 1));

		rg::RGTextureViewHandle outputTextureView = bloomTextureMips[passIdx];

		BloomPassInfo passInfo = CreateBloomPassInfo(inputRes, outputRes);
		if (passIdx == 0)
		{
			passInfo.isSetupPass = true;
		}

		const lib::MTHandle<BloomPassDS> bloomPassDS = graphBuilder.CreateDescriptorSet<BloomPassDS>(RENDERER_RESOURCE_NAME(std::format("BloomDownsampleDS(%d)", passIdx)));
		bloomPassDS->u_bloomInfo		= passInfo;
		bloomPassDS->u_inputTexture		= inputTextureView;
		bloomPassDS->u_outputTexture	= outputTextureView;

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

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingRes();

	const Int32 bloomPassesNum = static_cast<Int32>(bloomTextureMips.size());

	static const rdr::PipelineStateID upsamplePipeline = CompileBloomUpsamplePipeline();

	rg::RGTextureViewHandle inputTextureView = bloomTextureMips.back();

	// Upsample passes
	for (Int32 passIdx = static_cast<Int32>(bloomPassesNum) - 2; passIdx >= 0; --passIdx)
	{
		const math::Vector2u inputRes = math::Vector2u(renderingRes.x() >> (passIdx + 2), renderingRes.y() >> (passIdx + 2));
		const math::Vector2u outputRes = math::Vector2u(renderingRes.x() >> (passIdx + 1), renderingRes.y() >> (passIdx + 1));

		rg::RGTextureViewHandle outputTextureView = bloomTextureMips[passIdx];

		BloomPassInfo passInfo = CreateBloomPassInfo(inputRes, outputRes);

		const lib::MTHandle<BloomPassDS> bloomPassDS = graphBuilder.CreateDescriptorSet<BloomPassDS>(RENDERER_RESOURCE_NAME(std::format("BloomUpsampleDS(%d)", passIdx)));
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

static void BloomComposite(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle bloomTextureMip0, rg::RGTextureViewHandle lensFlaresTexture, rg::RGTextureViewHandle outputTexture)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingRes();

	static const rdr::PipelineStateID combinePipeline = CompileBloomCompositePipeline();

	const math::Vector2u inputRes = math::Vector2u(renderingRes.x() >> 1, renderingRes.y() >> 1);
	const math::Vector2u outputRes = math::Vector2u(renderingRes.x(), renderingRes.y());

	const BloomPassInfo passInfo = CreateBloomPassInfo(inputRes, outputRes);

	const lib::MTHandle<BloomPassDS> bloomPassDS = graphBuilder.CreateDescriptorSet<BloomPassDS>(RENDERER_RESOURCE_NAME("BloomCompositeDS"));
	bloomPassDS->u_inputTexture		= bloomTextureMip0;
	bloomPassDS->u_outputTexture	= outputTexture;
	bloomPassDS->u_bloomInfo		= passInfo;

	BloomCompositePassInfo compositePassInfo;
	compositePassInfo.bloomBlendFactor		= params::bloomBlendFactor;
	compositePassInfo.lensFlaresIntensity	= params::lensFlaresIntensity;

	const lib::MTHandle<BloomCompositePassDS> bloomCompositePassDS = graphBuilder.CreateDescriptorSet<BloomCompositePassDS>(RENDERER_RESOURCE_NAME("BloomCombinePassDS"));

	const RenderSceneEntityHandle renderViewEntity = viewSpec.GetRenderView().GetViewEntity();
	if (const CameraLensSettingsComponent* lensSettings = renderViewEntity.try_get<CameraLensSettingsComponent>())
	{
		bloomCompositePassDS->u_lensDirtTexture = lensSettings->lensDirtTexture;
		compositePassInfo.hasLensDirtTexture	= true;
		compositePassInfo.lensDirtThreshold		= lensSettings->lensDirtThreshold;
		compositePassInfo.lensDirtIntensity		= lensSettings->lensDirtIntensity;
	}

	bloomCompositePassDS->u_lensFlaresTexture = lensFlaresTexture;
	compositePassInfo.hasLensFlaresTexture    = lensFlaresTexture.IsValid();

	bloomCompositePassDS->u_bloomCompositeInfo = compositePassInfo;

	const math::Vector3u groupCount(math::Utils::DivideCeil(renderingRes.x(), 8u), math::Utils::DivideCeil(renderingRes.y(), 8u), 1u);
	graphBuilder.Dispatch(RG_DEBUG_NAME("Bloom Composite"),
						  combinePipeline, 
						  groupCount,
						  rg::BindDescriptorSets(bloomPassDS, bloomCompositePassDS));
}

static void ApplyBloom(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, rg::RGTextureViewHandle linearColorTexture)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingRes();
	const Uint32 bloomPassesNum = std::max(math::Utils::ComputeMipLevelsNumForResolution(renderingRes), 6u) - 5u;

	rg::TextureDef bloomTextureDef;
	bloomTextureDef.resolution	= math::Vector3u(renderingRes.x() / 2, renderingRes.y() / 2, 1);
	bloomTextureDef.format		= SceneRendererStatics::hdrFormat;
	bloomTextureDef.samples		= 1;
	bloomTextureDef.mipLevels	= bloomPassesNum;
	bloomTextureDef.arrayLayers	= 1;
	const rg::RGTextureHandle bloomTexture = graphBuilder.CreateTexture(RG_DEBUG_NAME("Bloom Downsample Texture"), bloomTextureDef);

	lib::DynamicArray<rg::RGTextureViewHandle> bloomTextureMips;
	bloomTextureMips.reserve(static_cast<SizeType>(bloomPassesNum));
	for (SizeType mipIdx = 0; mipIdx < bloomPassesNum; ++mipIdx)
	{
		rhi::TextureViewDefinition viewInfo;
		viewInfo.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color, static_cast<Uint32>(mipIdx), 1);

		bloomTextureMips.emplace_back(graphBuilder.CreateTextureView(RG_DEBUG_NAME(std::format("Bloom Texture Mip {}", mipIdx)), bloomTexture, viewInfo));
	}

	BloomDownsample(graphBuilder, viewSpec, bloomTextureMips, linearColorTexture);

	BloomUpsample(graphBuilder, viewSpec, bloomTextureMips);

	const rg::RGTextureViewHandle lensFlaresTexture = params::enableLensFlares
													? lens_flares::ComputeLensFlares(graphBuilder, viewSpec, bloomTextureMips[0])
													: rg::RGTextureViewHandle{};

	BloomComposite(graphBuilder, viewSpec, bloomTextureMips[0], lensFlaresTexture, linearColorTexture);
}

} // bloom

//////////////////////////////////////////////////////////////////////////////////////////////////
// Tonemapping ===================================================================================

namespace tonemapping
{

BEGIN_SHADER_STRUCT(TonemappingPassConstants)
	SHADER_STRUCT_FIELD(math::Vector2f, bilateralGridUVPerPixel)
	SHADER_STRUCT_FIELD(Bool,           enableColorDithering)
	SHADER_STRUCT_FIELD(Real32,         minLogLuminance)
	SHADER_STRUCT_FIELD(Real32,         logLuminanceRange)
	SHADER_STRUCT_FIELD(Real32,         inverseLogLuminanceRange)
	SHADER_STRUCT_FIELD(Real32,         contrastStrength)
	SHADER_STRUCT_FIELD(Real32,         detailStrength)
	SHADER_STRUCT_FIELD(Real32,         bilateralGridStrength)
END_SHADER_STRUCT();


DS_BEGIN(TonemappingDS, rg::RGDescriptorSetState<TonemappingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),								u_linearColorTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),								u_LDRTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector2f>),								u_luminanceBilateralGridTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_logLuminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TonemappingPassConstants>),					u_tonemappingConstants)
DS_END();


static rdr::PipelineStateID CompileTonemappingPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/Tonemapping.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TonemappingCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("TonemappingPipeline"), shader);
}


struct TonemappingParameters
{
	rg::RGTextureViewHandle linearColorTexture;
	rg::RGTextureViewHandle ldrTexture;
	rg::RGTextureViewHandle luminanceBilateralGridTexture;
	rg::RGTextureViewHandle logLuminanceTexture;
	TonemappingPassConstants tonemappingConstants;
};


static void DoTonemappingAndGammaCorrection(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, TonemappingParameters tonemappingParameters)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(tonemappingParameters.linearColorTexture.IsValid());
	SPT_CHECK(tonemappingParameters.ldrTexture.IsValid());
	SPT_CHECK(tonemappingParameters.luminanceBilateralGridTexture.IsValid());
	SPT_CHECK(tonemappingParameters.logLuminanceTexture.IsValid());

	const RenderView& renderView = viewSpec.GetRenderView();
	
	const lib::MTHandle<TonemappingDS> tonemappingDS = graphBuilder.CreateDescriptorSet<TonemappingDS>(RENDERER_RESOURCE_NAME("TonemappingDS"));
	tonemappingDS->u_linearColorTexture            = tonemappingParameters.linearColorTexture;
	tonemappingDS->u_LDRTexture                    = tonemappingParameters.ldrTexture;
	tonemappingDS->u_luminanceBilateralGridTexture = tonemappingParameters.luminanceBilateralGridTexture;
	tonemappingDS->u_logLuminanceTexture           = tonemappingParameters.logLuminanceTexture;
	tonemappingDS->u_tonemappingConstants          = tonemappingParameters.tonemappingConstants;

	static const rdr::PipelineStateID pipelineState = CompileTonemappingPipeline();

	const math::Vector2u renderingRes = renderView.GetRenderingRes();

	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(renderingRes.x(), 8u), math::Utils::DivideCeil(renderingRes.y(), 8u), 1u);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Tonemapping And Gamma"),
						  pipelineState,
						  dispatchGroupsNum,
						  rg::BindDescriptorSets(tonemappingDS,
												 renderView.GetRenderViewDS()));
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
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PostProcessing/GammaCorrection.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "GammaCorrectionCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("GammaCorrectionPipeline"), shader);
}

void DoGammaCorrection(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle texture)
{
	SPT_PROFILER_FUNCTION();
	
	const lib::MTHandle<GammaCorrectionDS> gammaCorrectionCS = graphBuilder.CreateDescriptorSet<GammaCorrectionDS>(RENDERER_RESOURCE_NAME("GammaCorrectionDS"));
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

void HDRResolveRenderStage::Initialize(RenderView& renderView)
{
	SPT_PROFILER_FUNCTION();

	Super::Initialize(renderView);

	rhi::BufferDefinition bufferDef;
	bufferDef.size  = sizeof(ViewExposureData);
	bufferDef.usage = lib::Flags(rhi::EBufferUsage::Uniform, rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	m_viewExposureBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("View Exposure Buffer"), bufferDef, rhi::EMemoryUsage::GPUOnly);

	const Real32 defaultExposure = 1.f;
	gfx::FillBuffer(lib::Ref(m_viewExposureBuffer), 0u, bufferDef.size, *reinterpret_cast<const Uint32*>(&defaultExposure));

	renderView.SetExposureDataBuffer(m_viewExposureBuffer);
}

void HDRResolveRenderStage::Deinitialize(RenderView& renderView)
{
	SPT_PROFILER_FUNCTION();

	Super::Deinitialize(renderView);

	renderView.ResetExposureDataBuffer();
}

void HDRResolveRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	ShadingViewContext& viewContext = viewSpec.GetShadingViewContext();

	const math::Vector2u renderingRes = viewSpec.GetRenderView().GetRenderingRes();
	const math::Vector3u textureRes(renderingRes.x(), renderingRes.y(), 1);
	const math::Vector2f inputPixelSize = math::Vector2f(1.f / static_cast<Real32>(renderingRes.x()), 1.f / static_cast<Real32>(renderingRes.y()));

	const rg::RGTextureViewHandle tonemappedTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("TonemappedTexture"), rg::TextureDef(textureRes, stageContext.rendererSettings.outputFormat));

	const rg::RGTextureViewHandle linearColorTexture = viewContext.luminance;

	const Real32 minLogLuminance = params::minLogLuminance;
	const Real32 logLuminanceRange = params::maxLogLuminance - params::minLogLuminance;

	bilateral_grid::builder::BilateralGridParams bilateralGridParams(viewSpec, linearColorTexture);
	bilateralGridParams.minLogLuminance   = minLogLuminance;
	bilateralGridParams.logLuminanceRange = logLuminanceRange;
	const auto [bilateralGridTexture, downsampledLogLuminanceTexture] = bilateral_grid::builder::RenderBilateralGrid(graphBuilder, bilateralGridParams);

	gaussian_blur_renderer::GaussianBlur3DParams gridBlurParams(params::bilateralGrid2DSigma, params::bilateralGridDepthSigma, params::bilateralGridBlurXKernel, params::bilateralGridBlurYKernel, params::bilateralGridBlurZKernel);
	const rg::RGTextureViewHandle bilateralGridBlurredTexture = gaussian_blur_renderer::ApplyGaussianBlur3D(graphBuilder, RG_DEBUG_NAME("Bilateral Grid Blur"), bilateralGridTexture, gridBlurParams);

	gaussian_blur_renderer::GaussianBlur2DParams logLuminanceBlurParams(params::logLuminanceBlurSigma, params::logLuminanceBlurXKernel, params::logLuminanceBlurYKernel);
	const rg::RGTextureViewHandle downsampledLogLuminanceBlurredTexture = gaussian_blur_renderer::ApplyGaussianBlur2D(graphBuilder, RG_DEBUG_NAME("Downsampled Log Luminance Blur"), downsampledLogLuminanceTexture, logLuminanceBlurParams);

	const rg::RGBufferViewHandle viewExposureData = graphBuilder.AcquireExternalBufferView(m_viewExposureBuffer->CreateFullView());

	exposure::ExposureSettings exposureSettings;
	exposureSettings.textureSize                    = renderingRes;
	exposureSettings.inputPixelSize                 = inputPixelSize;
	exposureSettings.deltaTime                      = renderScene.GetCurrentFrameRef().GetDeltaTime();
	exposureSettings.minLogLuminance                = minLogLuminance;
	exposureSettings.maxLogLuminance                = params::maxLogLuminance;
	exposureSettings.logLuminanceRange              = logLuminanceRange;
	exposureSettings.inverseLogLuminanceRange       = 1.f / exposureSettings.logLuminanceRange;
	exposureSettings.adaptationSpeed                = params::adaptationSpeed;
	exposureSettings.rejectedDarkPixelsPercentage   = params::rejectedDarkPixelsPercentage;
	exposureSettings.rejectedBrightPixelsPercentage = params::rejectedBrightPixelsPercentage;
	exposureSettings.evCompensation                 = params::evCompensation;

	const rg::RGBufferViewHandle luminanceHistogram = exposure::CreateLuminanceHistogram(graphBuilder, viewSpec, exposureSettings, viewContext.eyeAdaptationLuminance);
	exposure::ComputeAdaptedLuminance(graphBuilder, viewSpec, exposureSettings, luminanceHistogram, viewExposureData);

	if (params::enableBloom)
	{
		bloom::ApplyBloom(graphBuilder, viewSpec, linearColorTexture);
	}

	tonemapping::TonemappingPassConstants tonemappingSettings;
	tonemappingSettings.enableColorDithering     = params::enableColorDithering;
	tonemappingSettings.minLogLuminance          = minLogLuminance;
	tonemappingSettings.logLuminanceRange        = logLuminanceRange;
	tonemappingSettings.inverseLogLuminanceRange = 1.f / logLuminanceRange;
	tonemappingSettings.contrastStrength         = params::contrastStrength;
	tonemappingSettings.detailStrength           = params::detailStrength;
	tonemappingSettings.bilateralGridStrength    = params::bilateralGridStrength;
	tonemappingSettings.bilateralGridUVPerPixel  = bilateral_grid::GetGridTileSize().cast<Real32>().cwiseQuotient(bilateralGridTexture->GetResolution2D().cast<Real32>());

	tonemapping::TonemappingParameters tonemappingParameters;
	tonemappingParameters.linearColorTexture            = linearColorTexture;
	tonemappingParameters.ldrTexture                    = tonemappedTexture;
	tonemappingParameters.luminanceBilateralGridTexture = bilateralGridBlurredTexture;
	tonemappingParameters.logLuminanceTexture           = downsampledLogLuminanceBlurredTexture;
	tonemappingParameters.tonemappingConstants          = tonemappingSettings;

	tonemapping::DoTonemappingAndGammaCorrection(graphBuilder, viewSpec, tonemappingParameters);

	viewContext.output = tonemappedTexture;
	
	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
	
	RenderViewEntryContext context;
	context.texture = tonemappedTexture;
	viewSpec.GetRenderViewEntry(RenderViewEntryDelegates::RenderSceneDebugLayer).Broadcast(graphBuilder, renderScene, viewSpec, context);
}

} // spt::rsc
