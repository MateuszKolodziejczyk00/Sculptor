#include "AutomaticExposure.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderGraphBuilder.h"
#include "View/ViewRenderingSpec.h"
#include "Transfers/UploadUtils.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "SceneRenderer/Utils/GaussianBlurRenderer.h"
#include "SceneRenderer/RenderStages/Utils/BilateralGridRenderer.h"
#include "RenderScene.h"
#include "EngineFrame.h"


namespace spt::rsc
{

namespace automatic_exposure
{

namespace params
{

RendererFloatParameter adaptationSpeed("Adaptation Speed", { "Exposure" }, 6.0f, 0.f, 20.f);
RendererFloatParameter rejectedDarkPixelsPercentage("Rejected Dark Pixels Percentage", { "Exposure" }, 0.6f, 0.f, 0.8f);
RendererFloatParameter rejectedBrightPixelsPercentage("Rejected Bright Pixels Percentage", { "Exposure" }, 0.05f, 0.f, 0.1f);
RendererFloatParameter evCompensation("EV Compensation", { "Exposure" }, -0.48f, -10.f, 10.f);

RendererIntParameter bilateralGridBlurXKernel("Blur X Kernel", { "LocalTonemap", "BilateralGrid" }, 12, 0, 32);
RendererIntParameter bilateralGridBlurYKernel("Blur Y Kernel", { "LocalTonemap", "BilateralGrid" }, 12, 0, 32);
RendererIntParameter bilateralGridBlurZKernel("Blur Z Kernel", { "LocalTonemap", "BilateralGrid" }, 12, 0, 32);
RendererFloatParameter bilateralGrid2DSigma("Blur 2D Sigma", { "LocalTonemap", "BilateralGrid" }, 8.f, 0.f, 32.f);
RendererFloatParameter bilateralGridDepthSigma("Blur Depth Sigma", { "LocalTonemap", "BilateralGrid" }, 2.2f, 0.f, 32.f);

RendererIntParameter logLuminanceBlurXKernel("Blur X Kernel", { "LocalTonemap", "LogLuminance" }, 14, 0, 32);
RendererIntParameter logLuminanceBlurYKernel("Blur Y Kernel", { "LocalTonemap", "LogLuminance" }, 14, 0, 32);
RendererFloatParameter logLuminanceBlurSigma("Blur 2D Sigma", { "LocalTonemap", "LogLuminance" }, 8.f, 0.f, 32.f);

} // params

namespace luminance_histogram_internal
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


static rg::RGBufferViewHandle ComputeAdaptedLuminance(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const ExposureSettings& exposureSettings, rg::RGBufferViewHandle luminanceHistogram, rg::RGBufferViewHandle viewExposureData)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(luminanceHistogram.IsValid());

	RenderView& renderView = viewSpec.GetRenderView();

	ViewLuminanceData& viewLuminanceData = renderView.GetBlackboard().GetOrCreate<ViewLuminanceData>();
	
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

} // luminance_histogram_internal

namespace bilateral_grid_internal
{

static bilateral_grid::BilateralGridInfo RenderBilateralGrid(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const Inputs& inputs)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Bilateral Grid");

	bilateral_grid::builder::BilateralGridParams bilateralGridParams(viewSpec, inputs.linearColor);
	bilateralGridParams.minLogLuminance   = inputs.minLogLuminance;
	bilateralGridParams.logLuminanceRange = inputs.maxLogLuminance - inputs.minLogLuminance;
	bilateral_grid::BilateralGridInfo grid = bilateral_grid::builder::RenderBilateralGrid(graphBuilder, bilateralGridParams);

	gaussian_blur_renderer::GaussianBlur3DParams gridBlurParams(params::bilateralGrid2DSigma, params::bilateralGridDepthSigma, params::bilateralGridBlurXKernel, params::bilateralGridBlurYKernel, params::bilateralGridBlurZKernel);
	grid.bilateralGrid = gaussian_blur_renderer::ApplyGaussianBlur3D(graphBuilder, RG_DEBUG_NAME("Bilateral Grid Blur"), grid.bilateralGrid, gridBlurParams);

	gaussian_blur_renderer::GaussianBlur2DParams logLuminanceBlurParams(params::logLuminanceBlurSigma, params::logLuminanceBlurXKernel, params::logLuminanceBlurYKernel);
	grid.downsampledLogLuminance = gaussian_blur_renderer::ApplyGaussianBlur2D(graphBuilder, RG_DEBUG_NAME("Downsampled Log Luminance Blur"), grid.downsampledLogLuminance, logLuminanceBlurParams);

	const math::Vector2u tileSize = bilateral_grid::GetGridTileSize();

	return grid;
}

} // bilateral_grid_internal


Outputs RenderAutoExposure(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const Inputs& inputs)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Auto Exposure");

	const bilateral_grid::BilateralGridInfo bilateralGrid = bilateral_grid_internal::RenderBilateralGrid(graphBuilder, viewSpec, inputs);

	const math::Vector2u textureSize = inputs.linearColor->GetResolution2D();

	luminance_histogram_internal::ExposureSettings exposureSettings;
	exposureSettings.textureSize                    = textureSize;
	exposureSettings.inputPixelSize                 = textureSize.cast<Real32>().cwiseInverse();
	exposureSettings.deltaTime                      = renderScene.GetCurrentFrameRef().GetDeltaTime();
	exposureSettings.minLogLuminance                = inputs.minLogLuminance;
	exposureSettings.maxLogLuminance                = inputs.maxLogLuminance;
	exposureSettings.logLuminanceRange              = inputs.maxLogLuminance - inputs.minLogLuminance;
	exposureSettings.inverseLogLuminanceRange       = 1.f / exposureSettings.logLuminanceRange;
	exposureSettings.adaptationSpeed                = params::adaptationSpeed;
	exposureSettings.rejectedDarkPixelsPercentage   = params::rejectedDarkPixelsPercentage;
	exposureSettings.rejectedBrightPixelsPercentage = params::rejectedBrightPixelsPercentage;
	exposureSettings.evCompensation                 = params::evCompensation;

	const rg::RGBufferViewHandle luminanceHistogram = luminance_histogram_internal::CreateLuminanceHistogram(graphBuilder, viewSpec, exposureSettings, inputs.linearColor);
	luminance_histogram_internal::ComputeAdaptedLuminance(graphBuilder, viewSpec, exposureSettings, luminanceHistogram, inputs.viewExposureData);

	Outputs outputs;
	outputs.bilateralGridInfo = bilateralGrid;

	return outputs;
}

} // automatic_exposure

} // spt::rsc
