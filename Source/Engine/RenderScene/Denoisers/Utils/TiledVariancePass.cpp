#include "TiledVariancePass.h"
#include "RenderGraphBuilder.h"
#include "MathUtils.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "ResourcesManager.h"


namespace spt::rsc::denoising::tiled_variance
{

namespace tiles_variance
{

DS_BEGIN(TilesVarianceDS, rg::RGDescriptorSetState<TilesVarianceDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),										u_tilesVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_inputValueTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
DS_END();


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

	const rhi::TextureDefinition tilesVarianceDef(tilesResolution, lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture), params.dataTexture->GetFormat());
	const rg::RGTextureViewHandle tilesVariance = graphBuilder.CreateTextureView(RG_DEBUG_NAME("TilesVariance"), tilesVarianceDef, rhi::EMemoryUsage::GPUOnly);

	lib::SharedPtr<TilesVarianceDS> ds = rdr::ResourcesManager::CreateDescriptorSetState<TilesVarianceDS>(RENDERER_RESOURCE_NAME("Tiles Variance DS"));
	ds->u_tilesVarianceTexture = tilesVariance;
	ds->u_inputValueTexture = params.dataTexture;

	static const rdr::PipelineStateID pipeline = CreateTilesVariancePipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Tiles Variance Filter", params.debugName.Get().ToString())),
						  pipeline,
						  tilesResolution,
						  rg::BindDescriptorSets(std::move(ds)));

	return tilesVariance;
}

} // tiles_variance

namespace variance_max
{

DS_BEGIN(TilesVarianceMax3x3DS, rg::RGDescriptorSetState<TilesVarianceMax3x3DS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_tilesVarianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),										u_tilesVarianceMax3x3Texture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
DS_END();


static rdr::PipelineStateID CreateTilesVarianceMax3x3Pipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/Filters/TilesVarianceMax3x3.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "TilesVarianceMaxCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Tiles Variance Max 3x3 Pipeline"), shader);
}


static rg::RGTextureViewHandle ComputeMaxVariance3x3(rg::RenderGraphBuilder& graphBuilder, const TiledVarianceParameters& params, rg::RGTextureViewHandle tilesVariance)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u tilesResolution = tilesVariance->GetResolution2D();

	const rhi::TextureDefinition tilesVariancMaxeDef(tilesResolution, lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture), params.dataTexture->GetFormat());
	const rg::RGTextureViewHandle tilesVarianceMax = graphBuilder.CreateTextureView(RG_DEBUG_NAME("TilesVarianceMax"), tilesVariancMaxeDef, rhi::EMemoryUsage::GPUOnly);

	const lib::SharedPtr<TilesVarianceMax3x3DS> ds = rdr::ResourcesManager::CreateDescriptorSetState<TilesVarianceMax3x3DS>(RENDERER_RESOURCE_NAME("Tiles Variance Max 3x3 DS"));
	ds->u_tilesVarianceTexture			= tilesVariance;
	ds->u_tilesVarianceMax3x3Texture	= tilesVarianceMax;

	static const rdr::PipelineStateID pipeline = CreateTilesVarianceMax3x3Pipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("{}: Tiles Variance Max 3x3 Filter", params.debugName.Get().ToString())),
						  pipeline,
						  tilesResolution,
						  rg::BindDescriptorSets(std::move(ds)));

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
