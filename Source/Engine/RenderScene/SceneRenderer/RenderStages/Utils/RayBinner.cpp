#include "RayBinner.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"


namespace spt::rsc::ray_binner
{

BEGIN_SHADER_STRUCT(RayBinningConstants)
	SHADER_STRUCT_FIELD(math::Vector2u, resolution)
END_SHADER_STRUCT();


DS_BEGIN(RayBinningDS, rg::RGDescriptorSetState<RayBinningDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),        u_rayDirections)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),                 u_rwReorderingsTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<RayBinningConstants>), u_constants)
DS_END();


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

	lib::MTHandle<RayBinningDS> rayBinningDS = graphBuilder.CreateDescriptorSet<RayBinningDS>(RENDERER_RESOURCE_NAME("RayBinningDS"));
	rayBinningDS->u_rayDirections        = rayDirectionsTexture;
	rayBinningDS->u_rwReorderingsTexture = reorderingsTexture;
	rayBinningDS->u_constants            = shaderConstants;

	const rdr::PipelineStateID rayBinningPipeline = CreateRayBinningPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Ray Binning"),
						  rayBinningPipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(16u, 16u)),
						  rg::BindDescriptorSets(std::move(rayBinningDS)));

	return reorderingsTexture;
}

} // spt::rsc::ray_binner
