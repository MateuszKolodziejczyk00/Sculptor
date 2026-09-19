#include "TiledVariancePass.h"
#include "RenderGraphBuilder.h"
#include "MathUtils.h"
#include "ResourcesManager.h"


namespace spt::rsc::denoising::tiled_variance
{

namespace tiles_variance
{

BEGIN_SHADER_STRUCT(TiledVarianceParams)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>, tilesVarianceTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>, inputValueTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateTilesVariancePipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/Filters/TilesVariance.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TilesVarianceCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Tiles Variance Pipeline"), shader);
}


static rg::RGTextureViewHandle ComputeTilesVariance(rg::RenderGraphBuilder& graphBuilder, const TiledVarianceParameters& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u inputResolution = params.dataTexture->GetResolution2D();

	const math::Vector2u tileSize = math::Vector2u::Constant(16u);
	const math::Vector2u tilesResolution = math::Utils::DivideCeil(inputResolution, tileSize);

	const rg::RGTextureViewHandle tilesVariance = graphBuilder.CreateTextureView(RG_DEBUG_NAME("TilesVariance"), rg::TextureDef(tilesResolution, params.dataTexture->GetFormat()));

	TiledVarianceParams shaderConstants;
	shaderConstants.tilesVarianceTexture = tilesVariance;
	shaderConstants.inputValueTexture    = params.dataTexture;

	static const rdr::PipelineStateID pipeline = CreateTilesVariancePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Tiles Variance Filter", params.debugName.Get().ToString())),
						  pipeline,
						  tilesResolution,
						  rg::ShaderParams(shaderConstants));

	return tilesVariance;
}

} // tiles_variance

namespace variance_max
{

BEGIN_SHADER_STRUCT(TilesVarianceMax3x3Params)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>, tilesVarianceTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>, tilesVarianceMax3x3Texture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateTilesVarianceMax3x3Pipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/Filters/TilesVarianceMax3x3.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TilesVarianceMaxCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Tiles Variance Max 3x3 Pipeline"), shader);
}


static rg::RGTextureViewHandle ComputeMaxVariance3x3(rg::RenderGraphBuilder& graphBuilder, const TiledVarianceParameters& params, rg::RGTextureViewHandle tilesVariance)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u tilesResolution = tilesVariance->GetResolution2D();

	const rg::RGTextureViewHandle tilesVarianceMax = graphBuilder.CreateTextureView(RG_DEBUG_NAME("TilesVarianceMax"), rg::TextureDef(tilesResolution, params.dataTexture->GetFormat()));

	TilesVarianceMax3x3Params shaderConstants;
	shaderConstants.tilesVarianceTexture       = tilesVariance;
	shaderConstants.tilesVarianceMax3x3Texture = tilesVarianceMax;

	static const rdr::PipelineStateID pipeline = CreateTilesVarianceMax3x3Pipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Tiles Variance Max 3x3 Filter", params.debugName.Get().ToString())),
						  pipeline,
						  tilesResolution,
						  rg::ShaderParams(shaderConstants));

	return tilesVarianceMax;
}

} // variance_max

rg::RGTextureViewHandle ComputeVariance(rg::RenderGraphBuilder& graphBuilder, const TiledVarianceParameters& params)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.dataTexture.IsValid());

	const rg::RGTextureViewHandle tilesVariance = tiles_variance::ComputeTilesVariance(graphBuilder, params);

	return variance_max::ComputeMaxVariance3x3(graphBuilder, params, tilesVariance);
}

} // spt::rsc::denoising::variance
