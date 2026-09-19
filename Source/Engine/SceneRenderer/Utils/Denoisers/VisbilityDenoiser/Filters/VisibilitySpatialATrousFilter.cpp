#include "VisibilitySpatialATrousFilter.h"
#include "ShaderStructs/ShaderStructs.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "View/RenderView.h"


namespace spt::rsc::visibility_denoiser::spatial
{

BEGIN_SHADER_STRUCT(SpatialATrousFilteringParams)
	SHADER_STRUCT_FIELD(Int32,                             samplesOffset)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         inputTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,         outputTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,         varianceTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         depthTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, normalsTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateSpatialATrousFilterPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/Visibility/VisibilitySpatialATrousFilter.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "SpatialATrousFilterCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Visibility Spatial A-Trous Filter Pipeline"), shader);
}


void ApplyATrousFilter(rg::RenderGraphBuilder& graphBuilder, const SpatialATrousFilterParams& params, rg::RGTextureViewHandle input, rg::RGTextureViewHandle output, Uint32 iterationIdx)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u resolution = output->GetResolution();

	static const rdr::PipelineStateID pipeline = CreateSpatialATrousFilterPipeline();

	SpatialATrousFilteringParams dispatchParams;
	dispatchParams.samplesOffset   = static_cast<Int32>(1u << iterationIdx);
	dispatchParams.inputTexture    = input;
	dispatchParams.outputTexture   = output;
	dispatchParams.varianceTexture = params.varianceTexture;
	dispatchParams.depthTexture    = params.depthTexture;
	dispatchParams.normalsTexture  = params.normalsTexture;
	
	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Denoise Spatial A-Trous Filter (Iteration {})", params.name.Get().ToString(), iterationIdx)),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector3u(8u, 8u, 1u)),
						  rg::ShaderParams(dispatchParams));
}

} // spt::rsc::visibility_denoiser::spatial
