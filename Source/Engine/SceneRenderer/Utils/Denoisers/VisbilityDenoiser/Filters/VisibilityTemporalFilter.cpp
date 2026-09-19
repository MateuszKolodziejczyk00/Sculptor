#include "VisibilityTemporalFilter.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"


namespace spt::rsc::visibility_denoiser::temporal_accumulation
{

BEGIN_SHADER_STRUCT(TemporalFilterShaderParams)
	SHADER_STRUCT_FIELD(Real32,                               currentFrameDefaultWeight)
	SHADER_STRUCT_FIELD(Real32,                               accumulatedFramesMaxCount)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<Real32>,         varianceTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<Real32>,         currentTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,         historyTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,         historyDepthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>,         depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>, motionTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>, normalsTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Uint32>,            accumulatedSamplesNumTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>,            accumulatedSamplesNumHistoryTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,            spatialMomentsTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector2f>, temporalMomentsTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector2f>, temporalMomentsHistoryTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateTemporalAccumulationPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/Visibility/VisibilityTemporalFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TemporalFilterCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Visibility Temporal Filter Pipeline"), shader);
}


void ApplyTemporalFilter(rg::RenderGraphBuilder& graphBuilder, const TemporalFilterParams& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.HasValidHistory());
	SPT_CHECK(params.accumulatedSamplesNumTexture.IsValid());
	SPT_CHECK(params.accumulatedSamplesNumHistoryTexture.IsValid());

	const math::Vector3u resolution = params.currentTexture->GetResolution();

	TemporalFilterShaderParams shaderParams;
	shaderParams.currentFrameDefaultWeight           = params.currentFrameDefaultWeight;
	shaderParams.accumulatedFramesMaxCount           = params.accumulatedFramesMaxCount;
	shaderParams.currentTexture                      = params.currentTexture;
	shaderParams.varianceTexture                     = params.varianceTexture;
	shaderParams.historyTexture                      = params.historyTexture;
	shaderParams.historyDepthTexture                 = params.historyDepthTexture;
	shaderParams.depthTexture                        = params.currentDepthTexture;
	shaderParams.motionTexture                       = params.motionTexture;
	shaderParams.normalsTexture                      = params.normalsTexture;
	shaderParams.accumulatedSamplesNumTexture        = params.accumulatedSamplesNumTexture;
	shaderParams.accumulatedSamplesNumHistoryTexture = params.accumulatedSamplesNumHistoryTexture;
	shaderParams.spatialMomentsTexture               = params.spatialMomentsTexture;
	shaderParams.temporalMomentsTexture              = params.temporalMomentsTexture;
	shaderParams.temporalMomentsHistoryTexture       = params.temporalMomentsHistoryTexture;

	const rdr::PipelineStateID pipeline = CreateTemporalAccumulationPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Denoise Temporal Filter", params.name.Get().ToString())),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::ShaderParams(shaderParams));
}

} // spt::rsc::visibility_denoiser::temporal_accumulation
