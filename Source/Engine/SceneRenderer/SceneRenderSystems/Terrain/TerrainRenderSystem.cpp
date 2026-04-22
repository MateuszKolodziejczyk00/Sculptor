#include "TerrainRenderSystem.h"
#include "Parameters/SceneRendererParams.h"
#include "RenderSceneConstants.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "Utils/TransfersUtils.h"
#include "Types/Buffer.h"
#include "Loaders/TextureLoader.h"
#include "Engine.h"
#include "MaterialsSubsystem.h"


namespace spt::rsc
{

SPT_REGISTER_SCENE_RENDER_SYSTEM(TerrainRenderSystem);

namespace renderer_params
{
RendererBoolParameter enableTerrain("Enable Terrain", { "Terrain" }, false);
} // renderer_params

namespace terrain_renderer
{

BEGIN_SHADER_STRUCT(TerrainDrawMeshTaskCommand)
	SHADER_STRUCT_FIELD(Uint32, dispatchGroupsX)
	SHADER_STRUCT_FIELD(Uint32, dispatchGroupsY)
	SHADER_STRUCT_FIELD(Uint32, dispatchGroupsZ)
	SHADER_STRUCT_FIELD(Uint32, visibleTileIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(TerrainBuildTileDrawCommandsConstants)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<TerrainDrawMeshTaskCommand>, rwDrawCommands)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<Uint32>,                     rwDrawCommandsCount)
END_SHADER_STRUCT();


SIMPLE_COMPUTE_PSO(TerrainBuildTileDrawCommandsPSO, "Sculptor/Terrain/TerrainBuildTileDrawCommands.hlsl", BuildTerrainTileDrawCommandsCS);


BEGIN_RG_NODE_PARAMETERS_STRUCT(TerrainIndirectDrawParams)
	RG_BUFFER_VIEW(drawCommands,      rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
	RG_BUFFER_VIEW(drawCommandsCount, rg::ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect)
END_RG_NODE_PARAMETERS_STRUCT();


TerrainIndirectDrawParams BuildTerrainDrawTilesCommandsBuffers(rg::RenderGraphBuilder& graphBuilder, Uint32 tilesNum)
{
	const Uint32 maxTilesNum = std::max(tilesNum, 1u);

	TerrainIndirectDrawParams buffers;
	buffers.drawCommands       = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Terrain Draw Commands"), sizeof(TerrainDrawMeshTaskCommand) * maxTilesNum);
	buffers.drawCommandsCount  = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Terrain Draw Commands Count"), sizeof(Uint32));

	graphBuilder.MemZeroBuffer(buffers.drawCommandsCount);

	TerrainBuildTileDrawCommandsConstants shaderConstants;
	shaderConstants.rwDrawCommands      = buffers.drawCommands;
	shaderConstants.rwDrawCommandsCount = buffers.drawCommandsCount;

	graphBuilder.Dispatch(RG_DEBUG_NAME("Build Terrain Tile Draw Commands"),
						  TerrainBuildTileDrawCommandsPSO::pso,
						  math::Vector3u(math::Utils::DivideCeil(tilesNum, 64u), 1u, 1u),
						  rg::EmptyDescriptorSets(),
						  shaderConstants);

	return buffers;
}


BEGIN_SHADER_STRUCT(TerrainRenderConstants)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<TerrainDrawMeshTaskCommand>, drawCommands)
END_SHADER_STRUCT();


DS_BEGIN(TerrainVisibilityDS, rg::RGDescriptorSetState<TerrainVisibilityDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<TerrainRenderConstants>), u_constants)
DS_END()


GRAPHICS_PSO(TerrainVisibilityPSO)
{
	MESH_SHADER("Sculptor/Terrain/TerrainDrawTiles.hlsl", Terrain_MS);
	FRAGMENT_SHADER("Sculptor/Terrain/TerrainDrawTiles.hlsl", TerrainVisibility_FS);

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		rhi::GraphicsPipelineDefinition psoDef;
		psoDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
		psoDef.rasterizationDefinition.cullMode = rhi::ECullMode::None;
		psoDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(
			rhi::ColorRenderTargetDefinition
			{
				.format         = rhi::EFragmentFormat::R32_U_Int,
				.colorBlendType = rhi::ERenderTargetBlendType::Disabled,
				.alphaBlendType = rhi::ERenderTargetBlendType::Disabled,
			});

		psoDef.renderTargetsDefinition.depthRTDefinition = rhi::DepthRenderTargetDefinition(rhi::EFragmentFormat::D32_S_Float, rhi::ECompareOp::Greater);
		pso = CompilePSO(compiler, psoDef, {});
	}
};


GRAPHICS_PSO(TerrainDepthPSO)
{
	MESH_SHADER("Sculptor/Terrain/TerrainDrawTiles.hlsl", Terrain_MS);
	FRAGMENT_SHADER("Sculptor/Terrain/TerrainDrawTiles.hlsl", TerrainDepth_FS);

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		rhi::GraphicsPipelineDefinition psoDef;
		psoDef.primitiveTopology = rhi::EPrimitiveTopology::TriangleList;
		psoDef.rasterizationDefinition.cullMode = rhi::ECullMode::None;
		psoDef.renderTargetsDefinition.depthRTDefinition = rhi::DepthRenderTargetDefinition(rhi::EFragmentFormat::D32_S_Float, rhi::ECompareOp::Greater);
		pso = CompilePSO(compiler, psoDef, {});
	}
};


void RenderVisibilityBuffer(rg::RenderGraphBuilder& graphBuilder, const TerrainVisibilityRenderParams& params, const TerrainRenderConstants& constants, const TerrainIndirectDrawParams& indirectDrawParams, Uint32 maxDrawsCount)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.depthTexture.IsValid());
	SPT_CHECK(params.visibilityTexture.IsValid());
	SPT_CHECK(params.depthTexture->GetResolution2D() == params.visibilityTexture->GetResolution2D());

	const math::Vector2u resolution = params.viewSpec.GetRenderingRes();

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView    = params.depthTexture;
	depthRTDef.loadOperation  = rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation = rhi::ERTStoreOperation::Store;

	rg::RGRenderTargetDef visibilityRTDef;
	visibilityRTDef.textureView    = params.visibilityTexture;
	visibilityRTDef.loadOperation  = rhi::ERTLoadOperation::Load;
	visibilityRTDef.storeOperation = rhi::ERTStoreOperation::Store;

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), resolution);
	renderPassDef.SetDepthRenderTarget(depthRTDef);
	renderPassDef.AddColorRenderTarget(visibilityRTDef);

	lib::MTHandle<TerrainVisibilityDS> ds = graphBuilder.CreateDescriptorSet<TerrainVisibilityDS>(RENDERER_RESOURCE_NAME("Terrain Visibility DS"));
	ds->u_constants = constants;

	graphBuilder.RenderPass(RG_DEBUG_NAME("Terrain Visibility Buffer"),
							renderPassDef,
							rg::BindDescriptorSets(ds),
							std::tie(indirectDrawParams),
							[resolution, indirectDrawParams, maxDrawsCount](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f::Zero(), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u::Zero(), resolution));
								recorder.BindGraphicsPipeline(TerrainVisibilityPSO::pso);

								const rdr::BufferView& drawCommandsView      = *indirectDrawParams.drawCommands->GetResource();
								const rdr::BufferView& drawCommandsCountView = *indirectDrawParams.drawCommandsCount->GetResource();

								recorder.DrawMeshTasksIndirectCount(drawCommandsView.GetBuffer(),
																	drawCommandsView.GetOffset(),
																	sizeof(TerrainDrawMeshTaskCommand),
																	drawCommandsCountView.GetBuffer(),
																	drawCommandsCountView.GetOffset(),
																	maxDrawsCount);
							});
}


void RenderShadowMap(rg::RenderGraphBuilder& graphBuilder, const TerrainShadowMapRenderParams& params, const TerrainRenderConstants& constants, const TerrainIndirectDrawParams& indirectDrawParams, Uint32 maxDrawsCount)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(params.depthTexture.IsValid());

	const math::Vector2u resolution = params.viewSpec.GetRenderingRes();

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView    = params.depthTexture;
	depthRTDef.loadOperation  = rhi::ERTLoadOperation::Load;
	depthRTDef.storeOperation = rhi::ERTStoreOperation::Store;

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), resolution);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	lib::MTHandle<TerrainVisibilityDS> ds = graphBuilder.CreateDescriptorSet<TerrainVisibilityDS>(RENDERER_RESOURCE_NAME("Terrain Visibility DS"));
	ds->u_constants = constants;

	graphBuilder.RenderPass(RG_DEBUG_NAME("Terrain Shadow Map"),
							renderPassDef,
							rg::BindDescriptorSets(ds),
							std::tie(indirectDrawParams),
							[resolution, indirectDrawParams, maxDrawsCount](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
							{
								recorder.SetViewport(math::AlignedBox2f(math::Vector2f::Zero(), resolution.cast<Real32>()), 0.f, 1.f);
								recorder.SetScissor(math::AlignedBox2u(math::Vector2u::Zero(), resolution));
								recorder.BindGraphicsPipeline(TerrainDepthPSO::pso);

								const rdr::BufferView& drawCommandsView      = *indirectDrawParams.drawCommands->GetResource();
								const rdr::BufferView& drawCommandsCountView = *indirectDrawParams.drawCommandsCount->GetResource();

								recorder.DrawMeshTasksIndirectCount(drawCommandsView.GetBuffer(),
																	drawCommandsView.GetOffset(),
																	sizeof(TerrainDrawMeshTaskCommand),
																	drawCommandsCountView.GetBuffer(),
																	drawCommandsCountView.GetOffset(),
																	maxDrawsCount);
							});
}

} // namespace terrain_renderer

namespace terrain_consts
{
static constexpr Real32 tileSizeMeters      = 32.f;
static constexpr Real32 clipmapExtentMeters = 512.f;
static constexpr Uint32 lodsNum             = 1u;

static constexpr Uint32 meshletQuadsPerEdge    = 7u;
static constexpr Uint32 meshletVerticesPerEdge = meshletQuadsPerEdge + 1u;
static constexpr Uint32 meshletVerticesNum     = meshletVerticesPerEdge * meshletVerticesPerEdge;
static constexpr Uint32 meshletIndicesNum      = meshletQuadsPerEdge * meshletQuadsPerEdge * 6u;
} // terrain_consts

namespace utils
{

lib::DynamicArray<rdr::HLSLStorage<TerrainClipmapTileGPU>> BuildInitialClipmapTiles()
{
	lib::DynamicArray<rdr::HLSLStorage<TerrainClipmapTileGPU>> tiles;

	const Int32 radiusInTiles = Int32(std::ceil(terrain_consts::clipmapExtentMeters / terrain_consts::tileSizeMeters) + 0.5f);

	for (Int32 tileY = -radiusInTiles; tileY <= radiusInTiles; ++tileY)
	{
		for (Int32 tileX = -radiusInTiles; tileX <= radiusInTiles; ++tileX)
		{
			TerrainClipmapTileGPU tileData;
			tileData.tileCoordX = tileX;
			tileData.tileCoordY = tileY;

			tiles.emplace_back(tileData);
		}
	}

	return tiles;
}

lib::DynamicArray<rdr::HLSLStorage<math::Vector2f>> BuildMeshletVertices()
{
	const Real32 invMeshletVerticesPerEdge = 1.f / static_cast<Real32>(terrain_consts::meshletVerticesPerEdge - 1u);

	lib::DynamicArray<rdr::HLSLStorage<math::Vector2f>> vertices;
	vertices.reserve(terrain_consts::meshletVerticesNum);

	for (Uint32 y = 0u; y < terrain_consts::meshletVerticesPerEdge; ++y)
	{
		for (Uint32 x = 0u; x < terrain_consts::meshletVerticesPerEdge; ++x)
		{
			vertices.emplace_back(math::Vector2f(x * invMeshletVerticesPerEdge, y * invMeshletVerticesPerEdge));
		}
	}

	return vertices;
}

lib::DynamicArray<rdr::HLSLStorage<Uint32>> BuildMeshletIndices()
{
	lib::DynamicArray<rdr::HLSLStorage<Uint32>> indices;
	indices.reserve(terrain_consts::meshletIndicesNum);

	for (Uint32 y = 0u; y < terrain_consts::meshletQuadsPerEdge; ++y)
	{
		for (Uint32 x = 0u; x < terrain_consts::meshletQuadsPerEdge; ++x)
		{
			const Uint32 v00 = y * terrain_consts::meshletVerticesPerEdge + x;
			const Uint32 v10 = v00 + 1u;
			const Uint32 v01 = (y + 1u) * terrain_consts::meshletVerticesPerEdge + x;
			const Uint32 v11 = v01 + 1u;

			indices.emplace_back(v00);
			indices.emplace_back(v10);
			indices.emplace_back(v01);

			indices.emplace_back(v10);
			indices.emplace_back(v11);
			indices.emplace_back(v01);
		}
	}

	return indices;
}

} // utils


BEGIN_SHADER_STRUCT(TerrainMaterialData)
	SHADER_STRUCT_FIELD(Uint32, unused)
END_SHADER_STRUCT();


TerrainRenderSystem::TerrainRenderSystem(RenderScene& owningScene)
	: Super(owningScene)
{
	m_supportedStages = ERenderStage::VisibilityBuffer;
}

void TerrainRenderSystem::Update(const SceneUpdateContext& context)
{
	Super::Update(context);

	if (m_initialized)
	{
		return;
	}

	const lib::DynamicArray<rdr::HLSLStorage<TerrainClipmapTileGPU>> clipmapTiles = utils::BuildInitialClipmapTiles();
	const rhi::BufferDefinition clipmapTilesDef(std::max<Uint64>(1u, static_cast<Uint64>(clipmapTiles.size())) * sizeof(rdr::HLSLStorage<TerrainClipmapTileGPU>), rhi::EBufferUsage::Storage);
	m_tilesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Terrain Clipmap Tiles"), clipmapTilesDef, rhi::EMemoryUsage::CPUToGPU);
	rdr::UploadDataToBuffer(lib::Ref(m_tilesBuffer), 0u, reinterpret_cast<const Byte*>(clipmapTiles.data()), static_cast<Uint64>(clipmapTiles.size()) * sizeof(rdr::HLSLStorage<TerrainClipmapTileGPU>));

	const lib::DynamicArray<rdr::HLSLStorage<math::Vector2f>> meshletVertices = utils::BuildMeshletVertices();
	const rhi::BufferDefinition meshletVerticesDef(std::max<Uint64>(1u, static_cast<Uint64>(meshletVertices.size())) * sizeof(rdr::HLSLStorage<math::Vector2f>), rhi::EBufferUsage::Storage);
	m_meshletVerticesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Terrain Meshlet Vertices"), meshletVerticesDef, rhi::EMemoryUsage::CPUToGPU);
	rdr::UploadDataToBuffer(lib::Ref(m_meshletVerticesBuffer), 0u, reinterpret_cast<const Byte*>(meshletVertices.data()), static_cast<Uint64>(meshletVertices.size()) * sizeof(rdr::HLSLStorage<math::Vector2f>));

	const lib::DynamicArray<rdr::HLSLStorage<Uint32>> meshletIndices = utils::BuildMeshletIndices();
	const rhi::BufferDefinition meshletIndicesDef(std::max<Uint64>(1u, static_cast<Uint64>(meshletIndices.size())) * sizeof(rdr::HLSLStorage<Uint32>), rhi::EBufferUsage::Storage);
	m_meshletIndicesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Terrain Meshlet Indices"), meshletIndicesDef, rhi::EMemoryUsage::CPUToGPU);
	rdr::UploadDataToBuffer(lib::Ref(m_meshletIndicesBuffer), 0u, reinterpret_cast<const Byte*>(meshletIndices.data()), static_cast<Uint64>(meshletIndices.size()) * sizeof(rdr::HLSLStorage<Uint32>));

	const lib::String texturePath = (engn::GetEngine().GetPaths().contentPath / "Textures/heightmap.png").generic_string();
	lib::SharedPtr<rdr::Texture> heightMap = gfx::TextureLoader::LoadTexture(texturePath);
	if (heightMap)
	{
		m_heightMap = heightMap->CreateView(RENDERER_RESOURCE_NAME("Terrain Heightmap View"));
	}

	TerrainMaterialData materialData{};

	mat::MaterialDefinition materialDefinition;
	materialDefinition.name          = "Terrain Material";
	materialDefinition.customOpacity = false;
	materialDefinition.transparent   = false;
	materialDefinition.doubleSided   = true;
	materialDefinition.emissive      = false;

	ecs::EntityHandle material = mat::MaterialsSubsystem::Get().CreateMaterial(materialDefinition, materialData);
	const mat::MaterialProxyComponent& materialProxy = material.get<mat::MaterialProxyComponent>();
	m_terrainMaterialShader = materialProxy.params.shader;

	m_initialized = true;
}

void TerrainRenderSystem::UpdateGPUSceneData(RenderSceneConstants& sceneData)
{
	SPT_CHECK(m_initialized);

	TerrainHeightMap heightMapData;
	heightMapData.texture        = m_heightMap;
	heightMapData.res            = m_heightMap ? m_heightMap->GetResolution2D() : math::Vector2u{};
	heightMapData.invRes         = math::Vector2f::Ones().cwiseQuotient(heightMapData.res.cast<Real32>());
	heightMapData.spanMeters     = math::Vector2f::Constant(terrain_consts::clipmapExtentMeters * 8.f);
	heightMapData.invSpanMeters  = math::Vector2f::Ones().cwiseQuotient(heightMapData.spanMeters);
	heightMapData.metersPerTexel = heightMapData.spanMeters.cwiseQuotient(heightMapData.res.cast<Real32>());
	heightMapData.minHeight      = 0.f;
	heightMapData.maxHeight      = 40.f;

	TerrainSceneData terrainData;
	terrainData.heightMap           = heightMapData;
	terrainData.tiles               = m_tilesBuffer->GetFullView();
	terrainData.tilesNum            = static_cast<Uint32>(m_tilesBuffer->GetSize() / sizeof(rdr::HLSLStorage<TerrainClipmapTileGPU>));
	terrainData.lodsNum             = terrain_consts::lodsNum;
	terrainData.tileSizeMeters      = terrain_consts::tileSizeMeters;
	terrainData.clipmapExtentMeters = terrain_consts::clipmapExtentMeters;
	terrainData.meshletVertices     = m_meshletVerticesBuffer->GetFullView();
	terrainData.meshletIndices      = m_meshletIndicesBuffer->GetFullView();
	terrainData.meshletVerticesNum  = terrain_consts::meshletVerticesNum;
	terrainData.meshletIndicesNum   = terrain_consts::meshletIndicesNum;
	terrainData.meshletTranglesNum  = terrain_consts::meshletIndicesNum / 3u;

	sceneData.terrain = terrainData;
}

Bool TerrainRenderSystem::IsEnabled() const
{
	return renderer_params::enableTerrain;
}

void TerrainRenderSystem::RenderVisibilityBuffer(rg::RenderGraphBuilder& graphBuilder, const TerrainVisibilityRenderParams& params) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_initialized);

	const Uint32 tilesNum = static_cast<Uint32>(m_tilesBuffer->GetSize() / sizeof(rdr::HLSLStorage<TerrainClipmapTileGPU>));
	const terrain_renderer::TerrainIndirectDrawParams terrainDraws = terrain_renderer::BuildTerrainDrawTilesCommandsBuffers(graphBuilder, tilesNum);

	terrain_renderer::TerrainRenderConstants constants;
	constants.drawCommands = terrainDraws.drawCommands;

	terrain_renderer::RenderVisibilityBuffer(graphBuilder, params, constants, terrainDraws, std::max(tilesNum, 1u));
}

void TerrainRenderSystem::RenderShadowMap(rg::RenderGraphBuilder& graphBuilder, const TerrainShadowMapRenderParams& params) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(m_initialized);

	const Uint32 tilesNum = static_cast<Uint32>(m_tilesBuffer->GetSize() / sizeof(rdr::HLSLStorage<TerrainClipmapTileGPU>));
	const terrain_renderer::TerrainIndirectDrawParams terrainDraws = terrain_renderer::BuildTerrainDrawTilesCommandsBuffers(graphBuilder, tilesNum);

	terrain_renderer::TerrainRenderConstants constants;
	constants.drawCommands = terrainDraws.drawCommands;

	terrain_renderer::RenderShadowMap(graphBuilder, params, constants, terrainDraws, std::max(tilesNum, 1u));
}

} // spt::rsc
