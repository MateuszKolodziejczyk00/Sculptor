#include "MaterialsRenderer.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "RenderGraphBuilder.h"
#include "Utils/ViewRenderingSpec.h"


namespace spt::rsc
{

namespace materials_renderer
{

namespace material_depth_renderer
{

namespace constants
{
static constexpr rhi::EFragmentFormat materialDepthFormat = rhi::EFragmentFormat::D16_UN_Float;
} // constants

BEGIN_SHADER_STRUCT(MaterialDepthParams)
	SHADER_STRUCT_FIELD(math::Vector2f, screenResolution)
	SHADER_STRUCT_FIELD(Uint16,         terrainMaterialBatchIdx)
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

	const rhi::GraphicsPipelineDefinition pipelineDef
	{
		.renderTargetsDefinition
		{
			.depthRTDefinition = rhi::DepthRenderTargetDefinition
			{
				.format         = constants::materialDepthFormat,
				.depthCompareOp = rhi::ECompareOp::Always,
			}
		}
	};

	return rdr::ResourcesManager::CreateGfxPipeline(RENDERER_RESOURCE_NAME("Create Material Depth Pipeline"), shaders, pipelineDef);
}

rg::RGTextureViewHandle CreateMaterialDepthTexture(rg::RenderGraphBuilder& graphBuilder, const math::Vector2u& resolution)
{
	return graphBuilder.CreateTextureView(RG_DEBUG_NAME("Material Depth"), rg::TextureDef(resolution, constants::materialDepthFormat));
}

void RenderMaterialDepth(rg::RenderGraphBuilder& graphBuilder, const MaterialsPassDefinition& passDef, const MaterialRenderCommands& renderCommands, rg::RGTextureViewHandle materialDepth)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(passDef.visibilityTexture.IsValid());
	SPT_CHECK(materialDepth.IsValid());
	SPT_CHECK(passDef.visibleMeshlets.IsValid());
	SPT_CHECK(passDef.visibilityTexture->GetResolution() == materialDepth->GetResolution());

	const math::Vector2u resolution = materialDepth->GetResolution2D();

	static const rdr::PipelineStateID createMaterialDepthPipeline = CompileMaterialDepthPipeline();

	MaterialDepthParams materialDepthParams;
	materialDepthParams.screenResolution        = resolution.cast<Real32>();
	materialDepthParams.terrainMaterialBatchIdx = renderCommands.terrainMaterialBatchIdx;

	lib::MTHandle<CreateMaterialDepthDS> materialDepthDS = graphBuilder.CreateDescriptorSet<CreateMaterialDepthDS>(RENDERER_RESOURCE_NAME("Create Material Depth DS"));
	materialDepthDS->u_visibilityTexture   = passDef.visibilityTexture;
	materialDepthDS->u_visibleMeshlets     = passDef.visibleMeshlets;
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

BEGIN_SHADER_STRUCT(EmitGBufferConstants)
	SHADER_STRUCT_FIELD(math::Vector2f, tileSizeNDC)
	SHADER_STRUCT_FIELD(math::Vector2f, screenResolution)
	SHADER_STRUCT_FIELD(math::Vector2f, invScreenResolution)
	SHADER_STRUCT_FIELD(Uint32,         groupsPerRow)
END_SHADER_STRUCT();


DS_BEGIN(EmitGBufferDS, rg::RGDescriptorSetState<EmitGBufferDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<EmitGBufferConstants>), u_emitGBufferConstants)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2u>),         u_materialDepthTilesTexture)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<GPUVisibleMeshlet>),  u_visibleMeshlets)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                 u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),                 u_visibilityTexture)
DS_END();


BEGIN_SHADER_STRUCT(MaterialBatchConstants)
	SHADER_STRUCT_FIELD(Real32, materialBatchDepth)
	SHADER_STRUCT_FIELD(Uint32, materialBatchIdx)
END_SHADER_STRUCT();


DS_BEGIN(MaterialBatchDS, rg::RGDescriptorSetState<MaterialBatchDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<MaterialBatchConstants>), u_materialBatchConstants)
DS_END();


BEGIN_SHADER_STRUCT(EmitGBufferPermutation)
	SHADER_STRUCT_FIELD(MaterialBatchPermutation, MATERIAL_BATCH)
	SHADER_STRUCT_FIELD(Bool,                     ENABLE_POM)
END_SHADER_STRUCT();


GRAPHICS_PSO(EmitGBufferPSO)
{
	MESH_SHADER("Sculptor/GeometryRendering/EmitGBuffer.hlsl", EmitGBuffer_MS);
	FRAGMENT_SHADER("Sculptor/GeometryRendering/EmitGBuffer.hlsl", EmitGBuffer_FS);

	PERMUTATION_DOMAIN(EmitGBufferPermutation);
};


GRAPHICS_PSO(EmitTerrainGBufferPSO)
{
	MESH_SHADER("Sculptor/GeometryRendering/EmitGBuffer.hlsl", EmitGBuffer_MS);
	FRAGMENT_SHADER("Sculptor/GeometryRendering/EmitGBuffer.hlsl", EmitTerrainGBuffer_FS);

	PERMUTATION_DOMAIN(EmitGBufferPermutation);
};


template<typename TPSO>
rdr::PipelineStateID CreateMaterialPipeline(const MaterialsPassDefinition& passDef, const MaterialBatchPermutation& matPermutation)
{
	SPT_PROFILER_FUNCTION();

	const ShadingViewContext& shadingContext = passDef.viewSpec.GetShadingViewContext();

	rhi::GraphicsPipelineDefinition pipelineDef
	{
		.primitiveTopology = rhi::EPrimitiveTopology::TriangleList,
		.rasterizationDefinition =
		{
			.cullMode = rhi::ECullMode::None,
		},
		.renderTargetsDefinition =
		{
			.depthRTDefinition = rhi::DepthRenderTargetDefinition
			{
				.format           = material_depth_renderer::constants::materialDepthFormat,
				.depthCompareOp   = rhi::ECompareOp::Equal,
				.enableDepthWrite = false
			}
		}
	};

	std::transform(shadingContext.gBuffer.begin(), shadingContext.gBuffer.end(),
				   std::back_inserter(pipelineDef.renderTargetsDefinition.colorRTsDefinition),
				   [](const rg::RGTextureViewHandle& textureView)
				   {
				   	   return rhi::ColorRenderTargetDefinition
				   	   {
				   	      .format = textureView->GetFormat(),
				   	      .colorBlendType = rhi::ERenderTargetBlendType::Disabled,
				   	      .alphaBlendType = rhi::ERenderTargetBlendType::Disabled
				   	   };
				   });

	if (passDef.enablePOM)
	{
		pipelineDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(
			rhi::ColorRenderTargetDefinition
			{
				.format = passDef.pomDepth->GetFormat(),
				.colorBlendType = rhi::ERenderTargetBlendType::Disabled,
				.alphaBlendType = rhi::ERenderTargetBlendType::Disabled
			});
	}

	EmitGBufferPermutation permutation;
	permutation.MATERIAL_BATCH = matPermutation;
	permutation.ENABLE_POM     = passDef.enablePOM;

	return TPSO::GetPermutation(pipelineDef, permutation);
}


void AppendGeometryMaterialsRenderCommands(rg::RenderGraphBuilder& graphBuilder, const MaterialsPassDefinition& passDef, const GeometryPassDataCollection& geometryPassData, MaterialRenderCommands& renderCommands)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(renderCommands.geometryMaterialBatchesOffset == idxNone<Uint16>);

	renderCommands.geometryMaterialBatchesOffset = static_cast<Uint16>(renderCommands.commands.size());

	const lib::DynamicArray<MaterialBatch>& materialBatches = geometryPassData.materialBatches;

	renderCommands.commands.reserve(renderCommands.commands.size() + materialBatches.size());

	const Uint16 maxMaterialBatchIdx = std::numeric_limits<Uint16>::max();
	const Real32 materialDepthStep = 1.0f / maxMaterialBatchIdx;

	for (SizeType materialBatchIdx = 0u; materialBatchIdx < materialBatches.size(); ++materialBatchIdx)
	{
		SPT_CHECK(materialBatchIdx < std::numeric_limits<Uint16>::max());

		const Real32 materialBatchDepth = materialBatchIdx * materialDepthStep;

		MaterialBatchConstants materialBatchConstants;
		materialBatchConstants.materialBatchDepth = materialBatchDepth;
		materialBatchConstants.materialBatchIdx   = static_cast<Uint32>(materialBatchIdx);

		const lib::MTHandle<MaterialBatchDS> materialBatchDS = graphBuilder.CreateDescriptorSet<MaterialBatchDS>(RENDERER_RESOURCE_NAME("Material Batch DS"));
		materialBatchDS->u_materialBatchConstants = materialBatchConstants;

		MaterialRenderCommand& renderCommand = renderCommands.commands.emplace_back();
		renderCommand.pipelineState   = CreateMaterialPipeline<EmitGBufferPSO>(passDef, materialBatches[materialBatchIdx].permutation);
		renderCommand.materialBatchDS = std::move(materialBatchDS);
	}
}


void AppendTerrainMaterialsRenderCommand(rg::RenderGraphBuilder& graphBuilder, const MaterialsPassDefinition& passDef, const MaterialBatchPermutation& materialPermutation, MaterialRenderCommands& renderCommands)
{
	SPT_CHECK(renderCommands.terrainMaterialBatchIdx == idxNone<Uint16>);

	renderCommands.terrainMaterialBatchIdx = static_cast<Uint16>(renderCommands.commands.size());

	const Uint32 batchIdx = static_cast<Uint32>(renderCommands.commands.size());

	const Uint16 maxMaterialBatchIdx = std::numeric_limits<Uint16>::max();
	const Real32 materialDepthStep = 1.0f / maxMaterialBatchIdx;

	const Real32 materialBatchDepth = batchIdx * materialDepthStep;

	MaterialBatchConstants materialBatchConstants;
	materialBatchConstants.materialBatchDepth = materialBatchDepth;
	materialBatchConstants.materialBatchIdx   = static_cast<Uint32>(batchIdx);

	const lib::MTHandle<MaterialBatchDS> materialBatchDS = graphBuilder.CreateDescriptorSet<MaterialBatchDS>(RENDERER_RESOURCE_NAME("Material Batch DS"));
	materialBatchDS->u_materialBatchConstants = materialBatchConstants;

	MaterialRenderCommand& renderCommand = renderCommands.commands.emplace_back();
	renderCommand.pipelineState   = CreateMaterialPipeline<EmitTerrainGBufferPSO>(passDef, materialPermutation);
	renderCommand.materialBatchDS = std::move(materialBatchDS);
}


void RecordMaterialRenderCommands(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder, Uint32 groupsPerMaterial, const MaterialRenderCommands& renderCommands)
{
	SPT_PROFILER_FUNCTION();

	for (const MaterialRenderCommand& renderCommand : renderCommands.commands)
	{
		recorder.BindGraphicsPipeline(renderCommand.pipelineState);

		recorder.BindDescriptorSetState(renderCommand.materialBatchDS);

		recorder.DrawMeshTasks(math::Vector3u(groupsPerMaterial, 1u, 1u));

		recorder.UnbindDescriptorSetState(renderCommand.materialBatchDS);
	}
}


rg::RGRenderPassDefinition CreateEmitGBufferRenderPassDef(const ViewRenderingSpec& viewSpec, const MaterialsPassDefinition& passDef, const rg::RGTextureViewHandle& materialDepth)
{
	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), viewSpec.GetRenderingRes());

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView    = materialDepth;
	depthRTDef.loadOperation  = rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation = rhi::ERTStoreOperation::Store;
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	for (const rg::RGTextureViewHandle& gBufferTexture : viewSpec.GetShadingViewContext().gBuffer)
	{
		rg::RGRenderTargetDef gBufferRTDef;
		gBufferRTDef.textureView    = gBufferTexture;
		gBufferRTDef.loadOperation  = rhi::ERTLoadOperation::DontCare;
		gBufferRTDef.storeOperation = rhi::ERTStoreOperation::Store;
		renderPassDef.AddColorRenderTarget(gBufferRTDef);
	}

	if (passDef.enablePOM)
	{
		SPT_CHECK(passDef.pomDepth.IsValid());

		rg::RGRenderTargetDef pomDepthRTDef;
		pomDepthRTDef.textureView    = passDef.pomDepth;
		pomDepthRTDef.loadOperation  = rhi::ERTLoadOperation::DontCare;
		pomDepthRTDef.storeOperation = rhi::ERTStoreOperation::Store;
		renderPassDef.AddColorRenderTarget(pomDepthRTDef);
	}

	return renderPassDef;
}


void RenderMaterials(rg::RenderGraphBuilder& graphBuilder, const MaterialsPassDefinition& passDef, const MaterialRenderCommands& renderCommands)
{
	SPT_PROFILER_FUNCTION();

	const ViewRenderingSpec& viewSpec = passDef.viewSpec;
	
	const math::Vector2u resolution = viewSpec.GetRenderingRes();

	const rg::RGTextureViewHandle materialDepth = material_depth_renderer::CreateMaterialDepthTexture(graphBuilder, resolution);

	material_depth_renderer::RenderMaterialDepth(graphBuilder, passDef, renderCommands, materialDepth);

	const rg::RGTextureViewHandle materialTiles = material_depth_tiles_renderer::CreateMaterialDepthTilesTexture(graphBuilder, resolution);

	material_depth_tiles_renderer::RenderMaterialDepthTiles(graphBuilder, materialDepth, materialTiles);

	const Uint32 matDepthTileSize = material_depth_tiles_renderer::GetMaterialDepthTileSize();

	const math::Vector2u tilesPerGroup = math::Vector2u(8u, 4u);
	const math::Vector2u pixelsPerGroup = tilesPerGroup * matDepthTileSize;

	const math::Vector2u groupsPerMaterial2D = math::Utils::DivideCeil(resolution, pixelsPerGroup);
	const Uint32 groupsPerMaterial = groupsPerMaterial2D.x() * groupsPerMaterial2D.y();

	EmitGBufferConstants emitGBufferConstants;
	emitGBufferConstants.tileSizeNDC         = 2.f * math::Vector2f::Constant(static_cast<Real32>(matDepthTileSize)).cwiseQuotient(resolution.cast<Real32>());
	emitGBufferConstants.screenResolution    = resolution.cast<Real32>();
	emitGBufferConstants.invScreenResolution = math::Vector2f::Ones().cwiseQuotient(emitGBufferConstants.screenResolution);
	emitGBufferConstants.groupsPerRow        = groupsPerMaterial2D.x();

	lib::MTHandle<EmitGBufferDS> emitGBufferDS = graphBuilder.CreateDescriptorSet<EmitGBufferDS>(RENDERER_RESOURCE_NAME("Emit GBuffer DS"));
	emitGBufferDS->u_emitGBufferConstants      = emitGBufferConstants;
	emitGBufferDS->u_materialDepthTilesTexture = materialTiles;
	emitGBufferDS->u_visibleMeshlets           = passDef.visibleMeshlets;
	emitGBufferDS->u_depthTexture              = passDef.depthTexture;
	emitGBufferDS->u_visibilityTexture         = passDef.visibilityTexture;

	rg::RGRenderPassDefinition renderPassDef = CreateEmitGBufferRenderPassDef(viewSpec, passDef, materialDepth);

	graphBuilder.RenderPass(RG_DEBUG_NAME("Materials Pass"),
							renderPassDef,
							rg::BindDescriptorSets(emitGBufferDS),
							[resolution, groupsPerMaterial, commands = std::move(renderCommands)](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));

								RecordMaterialRenderCommands(renderContext, recorder, groupsPerMaterial, commands);
							});
}

} // materials_renderer

} // spt::rsc
