#include "AutomaticExposure.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RenderGraphBuilder.h"
#include "Utils/ViewRenderingSpec.h"
#include "Utils/TransfersUtils.h"
#include "SceneRenderer/Utils/GaussianBlurRenderer.h"
#include "SceneRenderer/RenderStages/Utils/BilateralGridRenderer.h"
#include "RenderScene.h"
#include "EngineFrame.h"
#include "ResourcesManager.h"


namespace spt::rsc
{

namespace automatic_exposure
{

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
	SHADER_STRUCT_FIELD(Real32, ec0)
	SHADER_STRUCT_FIELD(Real32, ec1)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(LuminanceHistogramConstants)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, linearColorTexture)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<ExposureSettings>,     exposureSettings)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Uint32>,        luminanceHistogram)
END_SHADER_STRUCT();


SIMPLE_COMPUTE_PSO(ComputeLuminanceHistogramPSO, "Sculptor/PostProcessing/CreateLuminanceHistogram.hlsl", LuminanceHistogramCS);


static rg::RGBufferViewHandle CreateLuminanceHistogram(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const rdr::GPUPtr<ExposureSettings>& exposureSettings, rg::RGTextureViewHandle linearColorTexture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(linearColorTexture.IsValid());

	const Uint64 histogramBinsNum = 256;

	const Uint64 histogramSize = sizeof(Uint32) * histogramBinsNum;
	const rhi::BufferDefinition luminanceHistogramDef(histogramSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
	const rg::RGBufferViewHandle luminanceHistogramBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("LuminanceHistogram"), luminanceHistogramDef, rhi::EMemoryUsage::GPUOnly);

	graphBuilder.FillBuffer(RG_DEBUG_NAME("Reset Luminance Histogram"), luminanceHistogramBuffer, 0, histogramSize, 0);
	
	LuminanceHistogramConstants consts;
	consts.linearColorTexture = linearColorTexture;
	consts.exposureSettings   = exposureSettings;
	consts.luminanceHistogram = luminanceHistogramBuffer;

	const math::Vector2u textureRes = linearColorTexture->GetResolution2D();
	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(textureRes.x(), 32u), math::Utils::DivideCeil(textureRes.y(), 32u), 1);
	graphBuilder.Dispatch(RG_DEBUG_NAME("Luminance Histogram"),
						  ComputeLuminanceHistogramPSO::pso,
						  dispatchGroupsNum,
						  rg::ShaderParams(consts));

	return luminanceHistogramBuffer;
}


struct ViewLuminanceData
{
	lib::SharedPtr<rdr::Buffer> adaptedLuminance;
};


BEGIN_SHADER_STRUCT(ComputeAdaptedLuminancePermutation)
	SHADER_STRUCT_FIELD(Bool, MANUAL_EXPOSURE)
END_SHADER_STRUCT();


COMPUTE_PSO(ComputeAdaptedLuminancePSO)
{
	PERMUTATION_DOMAIN(ComputeAdaptedLuminancePermutation);

	COMPUTE_SHADER("Sculptor/PostProcessing/ComputeAdaptedLuminance.hlsl", AdaptedLuminanceCS);

	PRESET(automatic);
	PRESET(manual);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		automatic = CompilePermutation(compiler, ComputeAdaptedLuminancePermutation{ .MANUAL_EXPOSURE = false });
		manual    = CompilePermutation(compiler, ComputeAdaptedLuminancePermutation{ .MANUAL_EXPOSURE = true });
	}
};


BEGIN_SHADER_STRUCT(ComputeAdaptedLuminanceConstants)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<ExposureSettings>,        exposureSettings)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<Uint32>,             luminanceHistogram)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<Real32>,           adaptedLuminance)
	SHADER_STRUCT_FIELD(gfx::RWTypedBuffer<ViewExposureData>, viewExposureData)
	SHADER_STRUCT_FIELD(Bool,                                 isFirstFrame)
	SHADER_STRUCT_FIELD(Real32,                               manualEV100)
END_SHADER_STRUCT();


static rg::RGBufferViewHandle ComputeAdaptedLuminance(rg::RenderGraphBuilder& graphBuilder, ViewRenderingSpec& viewSpec, const rdr::GPUPtr<ExposureSettings>& exposureSettings, rg::RGTextureViewHandle inputTexture, rg::RGBufferViewHandle viewExposureData)
{
	SPT_PROFILER_FUNCTION();

	RenderView& renderView = viewSpec.GetRenderView();

	const Bool isAutoExposure = viewSpec.GetSettings().exposure.enableAutoExposure;

	ViewLuminanceData& viewLuminanceData = renderView.GetBlackboard().GetOrCreate<ViewLuminanceData>();
	
	if (!viewLuminanceData.adaptedLuminance)
	{
		const Uint64 adaptedLuminanceBufferSize = sizeof(Real32);
		rhi::BufferDefinition adaptedLuminanceDef(adaptedLuminanceBufferSize, lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst));
		viewLuminanceData.adaptedLuminance = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("AdaptedLuminanceBuffer"), adaptedLuminanceDef, rhi::EMemoryUsage::GPUOnly);
		constexpr Real32 defaultAadptedLuminance = 0.18f;
		rdr::FillBuffer(lib::Ref(viewLuminanceData.adaptedLuminance), 0, sizeof(Real32), *reinterpret_cast<const Uint32*>(&defaultAadptedLuminance));
	}

	const rg::RGBufferViewHandle adaptedLuminanceBuffer = graphBuilder.AcquireExternalBufferView(viewLuminanceData.adaptedLuminance->GetFullView());

	const rg::RGBufferViewHandle luminanceHistogram = CreateLuminanceHistogram(graphBuilder, viewSpec, exposureSettings, inputTexture);

	const Bool isFirstFrame = viewSpec.GetFrameIdx() == 0u;

	ComputeAdaptedLuminanceConstants consts;
	consts.exposureSettings   = exposureSettings;
	consts.adaptedLuminance   = adaptedLuminanceBuffer;
	consts.viewExposureData   = viewExposureData;
	consts.isFirstFrame       = isFirstFrame;

	if (isAutoExposure)
	{
		consts.luminanceHistogram = luminanceHistogram;
	}
	else
	{
		consts.manualEV100 = renderView.settings.exposure.manualEV100;
	}

	const rdr::PipelineStateID pipelineState = isAutoExposure ? ComputeAdaptedLuminancePSO::automatic : ComputeAdaptedLuminancePSO::manual;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Adapted Luminance"), pipelineState, math::Vector3u(1, 1, 1), rg::ShaderParams(consts));

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

	const LocalTonemapSettings& settings = viewSpec.GetSettings().localTonemap;

	gaussian_blur_renderer::GaussianBlur3DParams gridBlurParams(settings.gridBlurSigma2D, settings.gridBlurDepthSigma, settings.gridBlurKernelX, settings.gridBlurKernelY, settings.gridBlurKernelZ);
	grid.bilateralGrid = gaussian_blur_renderer::ApplyGaussianBlur3D(graphBuilder, RG_DEBUG_NAME("Bilateral Grid Blur"), grid.bilateralGrid, gridBlurParams);

	gaussian_blur_renderer::GaussianBlur2DParams logLuminanceBlurParams(settings.luminanceBlurSigma, settings.luminanceBlurKernelX, settings.luminanceBlurKernelY);
	grid.downsampledLogLuminance = gaussian_blur_renderer::ApplyGaussianBlur2D(graphBuilder, RG_DEBUG_NAME("Downsampled Log Luminance Blur"), grid.downsampledLogLuminance, logLuminanceBlurParams);

	return grid;
}

} // bilateral_grid_internal


std::optional<LocalExposureOutputs> RenderAutoExposure(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const Inputs& inputs)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Auto Exposure");

	const math::Vector2u textureSize = inputs.linearColor->GetResolution2D();

	const rsc::RenderViewSettings& settings = viewSpec.GetSettings();

	luminance_histogram_internal::ExposureSettings exposureSettings;
	exposureSettings.textureSize                    = textureSize;
	exposureSettings.inputPixelSize                 = textureSize.cast<Real32>().cwiseInverse();
	exposureSettings.deltaTime                      = renderScene.GetCurrentFrameRef().GetDeltaTime();
	exposureSettings.minLogLuminance                = inputs.minLogLuminance;
	exposureSettings.maxLogLuminance                = inputs.maxLogLuminance;
	exposureSettings.logLuminanceRange              = inputs.maxLogLuminance - inputs.minLogLuminance;
	exposureSettings.inverseLogLuminanceRange       = 1.f / exposureSettings.logLuminanceRange;
	exposureSettings.adaptationSpeed                = settings.exposure.adaptationSpeed;
	exposureSettings.rejectedDarkPixelsPercentage   = settings.exposure.rejectedDarkPixels;
	exposureSettings.rejectedBrightPixelsPercentage = settings.exposure.rejectedBrightPixels;
	exposureSettings.ec0                            = settings.exposure.evCompensation0;
	exposureSettings.ec1                            = settings.exposure.evCompensation1;
	const rdr::GPUPtr<luminance_histogram_internal::ExposureSettings> exposureSettingsGPU = graphBuilder.CreateGPUData(exposureSettings);

	luminance_histogram_internal::ComputeAdaptedLuminance(graphBuilder, viewSpec, exposureSettingsGPU, inputs.linearColor, inputs.viewExposureData);

	if (settings.localTonemap.enableLocalExposure)
	{
		const bilateral_grid::BilateralGridInfo bilateralGrid = bilateral_grid_internal::RenderBilateralGrid(graphBuilder, viewSpec, inputs);

		LocalExposureOutputs outputs;
		outputs.bilateralGridInfo = bilateralGrid;

		return outputs;
	}
	else
	{
		return std::nullopt;
	}
}

} // automatic_exposure

} // spt::rsc
