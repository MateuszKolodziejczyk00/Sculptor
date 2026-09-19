#include "RayBinner.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "ResourcesManager.h"


namespace spt::rsc::ray_binner
{

BEGIN_SHADER_STRUCT(RayBinningConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                    resolution)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, rayDirections)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Uint32>,         rwReorderingsTexture)
END_SHADER_STRUCT();


static rdr::PipelineStateID CreateRayBinningPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/RayBinning/RayBinner.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "RayBinningCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("RayBinningPipeline"), shader);
}


rg::RGTextureViewHandle ExecuteRayBinning(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle rayDirectionsTexture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(rayDirectionsTexture.IsValid());

	const math::Vector2u resolution = rayDirectionsTexture->GetResolution2D();

	const rg::RGTextureViewHandle reorderingsTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Reoderings Texture"),
																					  rg::TextureDef(resolution, rhi::EFragmentFormat::R8_U_Int));

	RayBinningConstants shaderConstants;
	shaderConstants.resolution = resolution;
	shaderConstants.rayDirections        = rayDirectionsTexture;
	shaderConstants.rwReorderingsTexture = reorderingsTexture;

	const rdr::PipelineStateID rayBinningPipeline = CreateRayBinningPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Ray Binning"),
						  rayBinningPipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(16u, 16u)),
						  rg::ShaderParams(shaderConstants));

	return reorderingsTexture;
}

} // spt::rsc::ray_binner
