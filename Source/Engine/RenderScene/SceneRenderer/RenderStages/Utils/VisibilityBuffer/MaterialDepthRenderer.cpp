#include "MaterialDepthRenderer.h"
#include "RenderGraphBuilder.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "Geometry/GeometryTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc
{

namespace material_depth_renderer
{

namespace constants
{
static constexpr rhi::EFragmentFormat materialDepthFormat      = rhi::EFragmentFormat::D16_UN_Float;
} // constants

BEGIN_SHADER_STRUCT(MaterialDepthParams)
	SHADER_STRUCT_FIELD(math::Vector2f, screenResolution)
END_SHADER_STRUCT();


DS_BEGIN(CreateMaterialDepthDS, rg::RGDescriptorSetState<CreateMaterialDepthDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                u_visibilityTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GPUVisibleMeshlet>), u_visibleMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<MaterialDepthParams>), u_materialDepthParams)
DS_END();


static rdr::PipelineStateID CompileMaterialDepthPipeline()
{
	const lib::String shadersPath = "Sculptor/GeometryRendering/MaterialDepth.hlsl";

	rdr::GraphicsPipelineShaders shaders;
	shaders.vertexShader   = rdr::ResourcesManager::CreateShader(shadersPath, sc::ShaderStageCompilationDef(rhi::EShaderStage::Vertex, "MaterialDepthVS"));
	shaders.fragmentShader = rdr::ResourcesManager::CreateShader(shadersPath, sc::ShaderStageCompilationDef(rhi::EShaderStage::Fragment, "MaterialDepthFS"));

	rhi::GraphicsPipelineDefinition pipelineDef;
	pipelineDef.renderTargetsDefinition.depthRTDefinition = rhi::DepthRenderTargetDefinition(constants::materialDepthFormat, rhi::ECompareOp::Always);

	return rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("Create Material Depth Pipeline"), shaders, pipelineDef);
}

rg::RGTextureViewHandle CreateMaterialDepthTexture(rg::RenderGraphBuilder& graphBuilder, const math::Vector2u& resolution)
{
	return graphBuilder.CreateTextureView(RG_DEBUG_NAME("Material Depth"), rg::TextureDef(resolution, constants::materialDepthFormat));
}

void RenderMaterialDepth(rg::RenderGraphBuilder& graphBuilder, const MaterialDepthParameters& parameters, rg::RGTextureViewHandle materialDepth)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(parameters.visibilityTexture.IsValid());
	SPT_CHECK(materialDepth.IsValid());
	SPT_CHECK(parameters.visibleMeshlets.IsValid());
	SPT_CHECK(parameters.visibilityTexture->GetResolution() == materialDepth->GetResolution());

	const math::Vector2u resolution = materialDepth->GetResolution2D();

	static const rdr::PipelineStateID createMaterialDepthPipeline = CompileMaterialDepthPipeline();

	MaterialDepthParams materialDepthParams;
	materialDepthParams.screenResolution = resolution.cast<Real32>();

	lib::MTHandle<CreateMaterialDepthDS> materialDepthDS = graphBuilder.CreateDescriptorSet<CreateMaterialDepthDS>(RENDERER_RESOURCE_NAME("Create Material Depth DS"));
	materialDepthDS->u_visibilityTexture   = parameters.visibilityTexture;
	materialDepthDS->u_visibleMeshlets     = parameters.visibleMeshlets;
	materialDepthDS->u_materialDepthParams = materialDepthParams;

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView    = materialDepth;
	depthRTDef.loadOperation  = rhi::ERTLoadOperation::Clear;
	depthRTDef.storeOperation = rhi::ERTStoreOperation::Store;
	depthRTDef.clearColor     = rhi::ClearColor(1.f);

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), resolution);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	graphBuilder.RenderPass(RG_DEBUG_NAME("Render Material Depth"),
							renderPassDef,
							rg::BindDescriptorSets(std::move(materialDepthDS)),
							[resolution, pipeline = createMaterialDepthPipeline](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));

								recorder.BindGraphicsPipeline(pipeline);

								recorder.DrawInstances(3u, 1u);
							});
}

} // material_depth_renderer

namespace material_depth_tiles_renderer
{

namespace constants
{
static constexpr rhi::EFragmentFormat materialDepthTilesFormat = rhi::EFragmentFormat::RG16_U_Int;
static constexpr Uint32 materialDepthTileSize = 64u;
} // constants

BEGIN_SHADER_STRUCT(MaterialDepthTilesShaderParams)
	SHADER_STRUCT_FIELD(math::Vector2u, materialDepthResolution)
END_SHADER_STRUCT();


DS_BEGIN(CreateMaterialDepthTilesDS, rg::RGDescriptorSetState<CreateMaterialDepthTilesDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                           u_materialDepthTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2u>),                    u_materialDepthTilesTexture)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<MaterialDepthTilesShaderParams>), u_params)
DS_END();


static rdr::PipelineStateID CompileMaterialDepthTilesPipeline()
{
	rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/GeometryRendering/MaterialDepthTiles.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "MaterialDepthTilesCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Create Material Depth Tiles Pipeline"), shader);
}


Uint32 GetMaterialDepthTileSize()
{
	return constants::materialDepthTileSize;
}

rg::RGTextureViewHandle CreateMaterialDepthTilesTexture(rg::RenderGraphBuilder& graphBuilder, const math::Vector2u& resolution)
{
	const math::Vector2u tileSize = math::Vector2u::Constant(constants::materialDepthTileSize);
	const math::Vector2u tilesResolution = math::Utils::DivideCeil(resolution, tileSize);
	return graphBuilder.CreateTextureView(RG_DEBUG_NAME("Material Depth Tiles"), rg::TextureDef(tilesResolution, constants::materialDepthTilesFormat));
}

void RenderMaterialDepthTiles(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle materialDepth, rg::RGTextureViewHandle materialDepthTiles)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(materialDepth.IsValid());
	SPT_CHECK(materialDepthTiles.IsValid());

	MaterialDepthTilesShaderParams params;
	params.materialDepthResolution = materialDepth->GetResolution2D();

	lib::MTHandle<CreateMaterialDepthTilesDS> materialDepthTilesDS = graphBuilder.CreateDescriptorSet<CreateMaterialDepthTilesDS>(RENDERER_RESOURCE_NAME("Create Material Depth Tiles DS"));
	materialDepthTilesDS->u_materialDepthTexture      = materialDepth;
	materialDepthTilesDS->u_materialDepthTilesTexture = materialDepthTiles;
	materialDepthTilesDS->u_params                    = params;

	static const rdr::PipelineStateID createMaterialDepthTilesPipeline = CompileMaterialDepthTilesPipeline();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Render Material Depth Tiles"),
						  createMaterialDepthTilesPipeline,
						  materialDepthTiles->GetResolution2D(),
						  rg::BindDescriptorSets(std::move(materialDepthTilesDS)));
}

} // material_depth_tiles_renderer

} // spt::rsc
