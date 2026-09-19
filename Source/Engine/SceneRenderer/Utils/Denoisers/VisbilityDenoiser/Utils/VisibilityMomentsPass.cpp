#include "VisibilityMomentsPass.h"
#include "RenderGraphBuilder.h"
#include "MathUtils.h"
#include "ResourcesManager.h"


namespace spt::rsc::visibility_denoiser::moments
{

namespace compression
{

BEGIN_SHADER_STRUCT(VisibilityDataCompressionParams)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Uint32>,  compressedDataTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,  inputTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateCompressionPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/Visibility/VisibilityDataCompression.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "VisibilityDataCompressionCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Visibility Data Compression Pipeline"), shader);
}

rg::RGTextureViewHandle CompressTexture(rg::RenderGraphBuilder& graphBuilder, const VisibilityMomentsParameters& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u inputResolution = params.dataTexture->GetResolution2D();
	const math::Vector2u compressedResolution = math::Utils::DivideCeil(inputResolution, math::Vector2u(8u, 4u));

	const rg::RGTextureViewHandle compressedTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Compressed", params.debugName.AsString()),
																					 rg::TextureDef(compressedResolution, rhi::EFragmentFormat::R32_U_Int));
																					 

	static const rdr::PipelineStateID pipeline = CreateCompressionPipeline();

	VisibilityDataCompressionParams shaderConstants;
	shaderConstants.compressedDataTexture = compressedTexture;
	shaderConstants.inputTexture          = params.dataTexture;
	
	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{}: Compress Data", params.debugName.AsString()),
						  pipeline,
						  compressedResolution,
						  rg::ShaderParams(shaderConstants));

	return compressedTexture;

}

} // compression

namespace computation
{

BEGIN_SHADER_STRUCT(VisibilityMomentsComputationParams)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>, compressedDataTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>, momentsTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateComputationPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/Visibility/VisibilityDataMoments.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "VisibilityDataMomentsCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Visibility Data Moments Computation Pipeline"), shader);
}

rg::RGTextureViewHandle ComputeMoments(rg::RenderGraphBuilder& graphBuilder, const VisibilityMomentsParameters& params, rg::RGTextureViewHandle compressedData)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.dataTexture->GetResolution2D();

	const rg::RGTextureViewHandle momentsTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Moments", params.debugName.AsString()),
																				  rg::TextureDef(resolution, rhi::EFragmentFormat::R16_UN_Float));

	static const rdr::PipelineStateID pipeline = CreateComputationPipeline();

	VisibilityMomentsComputationParams shaderConstants;
	shaderConstants.compressedDataTexture = compressedData;
	shaderConstants.momentsTexture        = momentsTexture;

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{}: Compute Moments", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::ShaderParams(shaderConstants));

	return momentsTexture;
}

} // computation

rg::RGTextureViewHandle ComputeMoments(rg::RenderGraphBuilder& graphBuilder, const VisibilityMomentsParameters& params)
{
	const rg::RGTextureViewHandle compressedTexture = compression::CompressTexture(graphBuilder, params);

	return computation::ComputeMoments(graphBuilder, params, compressedTexture);
}

} // spt::rsc::visibility_denoiser::moments
