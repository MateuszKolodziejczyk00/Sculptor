#include "TerrainRenderSystem.h"
#include "EngineFrame.h"
#include "Parameters/SceneRendererParams.h"
#include "RenderSceneConstants.h"
#include "RenderGraphBuilder.h"
#include "ResourcesManager.h"
#include "Utils/TransfersManager.h"
#include "Utils/TransfersUtils.h"
#include "Types/Buffer.h"
#include "Engine.h"
#include "MaterialsSubsystem.h"


namespace spt::rsc
{

namespace terrain_consts
{
static constexpr Real32 tileSizeMeters      = 32.f;
static constexpr Real32 clipmapExtentMeters = 1024.f;

static constexpr Uint32 meshletQuadsPerEdge    = 8u;
static constexpr Uint32 meshletVerticesPerEdge = meshletQuadsPerEdge + 1u;
static constexpr Uint32 meshletVerticesNum     = meshletVerticesPerEdge * meshletVerticesPerEdge;
static constexpr Uint32 meshletIndicesNum      = meshletQuadsPerEdge * meshletQuadsPerEdge * 6u;

static const math::Vector2u materialCacheRes = math::Vector2u(2048u, 2048u);
} // terrain_consts


SPT_REGISTER_SCENE_RENDER_SYSTEM(TerrainRenderSystem);


namespace renderer_params
{
RendererBoolParameter enableTerrain("Enable Terrain", { "Terrain" }, false);
} // renderer_params


namespace utils
{

math::Vector2u ComputeTilesResolution()
{
	return math::Vector2u(
		static_cast<Uint32>(std::ceil(terrain_consts::clipmapExtentMeters / terrain_consts::tileSizeMeters) * 2.f),
		static_cast<Uint32>(std::ceil(terrain_consts::clipmapExtentMeters / terrain_consts::tileSizeMeters) * 2.f));
}


lib::DynamicArray<rdr::HLSLStorage<TerrainClipmapTileGPU>> BuildInitialClipmapTiles()
{
	lib::DynamicArray<rdr::HLSLStorage<TerrainClipmapTileGPU>> tiles;

	const Int32 radiusInTiles = Int32(std::ceil(terrain_consts::clipmapExtentMeters / terrain_consts::tileSizeMeters) + 0.5f);

	for (Int32 tileY = -radiusInTiles; tileY < radiusInTiles; ++tileY)
	{
		for (Int32 tileX = -radiusInTiles; tileX < radiusInTiles; ++tileX)
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


lib::DynamicArray<rdr::HLSLStorage<Uint32>> BuildTileIndices(Uint32 tileVerticesPerEdge)
{
	SPT_CHECK(tileVerticesPerEdge >= 2u);

	const Uint32 quadsPerEdge = tileVerticesPerEdge - 1u;

	lib::DynamicArray<rdr::HLSLStorage<Uint32>> indices;
	indices.reserve(quadsPerEdge * quadsPerEdge * 6u);

	for (Uint32 y = 0u; y < quadsPerEdge; ++y)
	{
		for (Uint32 x = 0u; x < quadsPerEdge; ++x)
		{
			const Uint32 v00 = y * tileVerticesPerEdge + x;
			const Uint32 v10 = v00 + 1u;
			const Uint32 v01 = (y + 1u) * tileVerticesPerEdge + x;
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


Uint32 MeshletsNumToVerticesPerEdge(Uint32 meshletsNumPerEdge)
{
	return meshletsNumPerEdge * (terrain_consts::meshletVerticesPerEdge - 1u) + 1u;
}


lib::DynamicArray<rdr::HLSLStorage<Uint32>> BuildMeshletIndices()
{
	return BuildTileIndices(terrain_consts::meshletVerticesPerEdge);
}

} // utils


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


namespace material_cache
{

struct MaterialCacheState
{
	struct LOD
	{
		lib::SharedPtr<rdr::TextureView> baseColorMetallic;
		lib::SharedPtr<rdr::TextureView> normals;
		lib::SharedPtr<rdr::TextureView> roughnessOcclusion;
		lib::SharedPtr<rdr::TextureView> pomDepth;

		Real32 sizeMeters = 0.f;

		math::Vector2f minBounds = math::Vector2f::Zero();
		math::Vector2f maxBounds = math::Vector2f::Zero();
	};

	lib::StaticArray<LOD, terrain_consts::materialCacheLODsNum> lods;
};


void InitializeMaterialCache(MaterialCacheState& state)
{
	SPT_PROFILER_FUNCTION();

	for (Uint32 i = 0u; i < terrain_consts::materialCacheLODsNum; ++i)
	{
		MaterialCacheState::LOD& lodCache = state.lods[i];

		rhi::TextureDefinition textureDefinition;
		textureDefinition.resolution = terrain_consts::materialCacheRes;
		textureDefinition.format     = rhi::EFragmentFormat::RGBA8_UN_Float;
		textureDefinition.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi:: ETextureUsage::ColorRT, rhi::ETextureUsage::TransferDest);
		textureDefinition.flags      = rhi::ETextureFlags::GloballyReadable;
		lodCache.baseColorMetallic = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Terrain Base Color Cache"), textureDefinition, rhi::EMemoryUsage::GPUOnly);

		textureDefinition.format = rhi::EFragmentFormat::RG16_UN_Float;
		lodCache.normals = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Terrain Normals Cache"), textureDefinition, rhi::EMemoryUsage::GPUOnly);

		textureDefinition.format = rhi::EFragmentFormat::RG8_UN_Float;
		lodCache.roughnessOcclusion = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Terrain Roughness/Occlusion Cache"), textureDefinition, rhi::EMemoryUsage::GPUOnly);

		lodCache.sizeMeters = 7.5f * std::pow(2.f, static_cast<Real32>(i));
	}

	{
		rhi::TextureDefinition textureDefinition;
		textureDefinition.resolution = terrain_consts::materialCacheRes;
		textureDefinition.format     = rhi::EFragmentFormat::RGBA8_UN_Float;
		textureDefinition.usage      = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi:: ETextureUsage::ColorRT, rhi::ETextureUsage::TransferDest);
		textureDefinition.flags      = rhi::ETextureFlags::GloballyReadable;

		constexpr Uint32 lodsWithPOMs = 2u;
		for (Uint32 i = 0u; i < lodsWithPOMs; ++i)
		{
			state.lods[i].pomDepth = rdr::ResourcesManager::CreateTextureView(RENDERER_RESOURCE_NAME("Terrain POM Depth Cache"), textureDefinition, rhi::EMemoryUsage::GPUOnly);
		}
	}
}


struct UpdateMaterialCacheParams
{
	MaterialCacheState::LOD* lod = nullptr;

	math::Vector2f prevMinBounds = math::Vector2f::Zero();
	math::Vector2f prevMaxBounds = math::Vector2f::Zero();
};


GRAPHICS_PSO(RenderCacheDepthTexturePSO)
{
	VERTEX_SHADER("Sculptor/Terrain/RenderCacheDepthTexture.hlsl", FullScreenVS);
	FRAGMENT_SHADER("Sculptor/Terrain/RenderCacheDepthTexture.hlsl", RenderCacheDepthTextureFS);

	PRESET(pso);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		rhi::GraphicsPipelineDefinition psoDef;
		psoDef.rasterizationDefinition.cullMode = rhi::ECullMode::None;
		psoDef.renderTargetsDefinition.depthRTDefinition = rhi::DepthRenderTargetDefinition(rhi::EFragmentFormat::D32_S_Float, rhi::ECompareOp::Always);
		pso = CompilePSO(compiler, psoDef, {});
	}
};


BEGIN_SHADER_STRUCT(RenderCacheDepthTextureConstants)
	SHADER_STRUCT_FIELD(math::Vector2f, prevMinBounds)
	SHADER_STRUCT_FIELD(math::Vector2f, prevMaxBounds)
	SHADER_STRUCT_FIELD(math::Vector2f, newMinBounds)
	SHADER_STRUCT_FIELD(math::Vector2f, newMaxBounds)
	SHADER_STRUCT_FIELD(math::Vector2f, rcpLodRange)
END_SHADER_STRUCT();


rg::RGTextureViewHandle RenderCacheDepthTexture(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const UpdateMaterialCacheParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = terrain_consts::materialCacheRes;

	const rg::RGTextureViewHandle cacheDepth = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Terrain Material Cache Depth"), rg::TextureDef(resolution, rhi::EFragmentFormat::D16_UN_Float));

	RenderCacheDepthTextureConstants shaderConstants;
	shaderConstants.prevMinBounds = params.prevMinBounds;
	shaderConstants.prevMaxBounds = params.prevMaxBounds;
	shaderConstants.newMinBounds  = params.lod->minBounds;
	shaderConstants.newMaxBounds  = params.lod->maxBounds;
	shaderConstants.rcpLodRange   = params.lod->maxBounds - params.lod->minBounds;

	rg::RGRenderTargetDef depthRTDef;
	depthRTDef.textureView    = cacheDepth;
	depthRTDef.loadOperation  = rhi::ERTLoadOperation::Clear;
	depthRTDef.storeOperation = rhi::ERTStoreOperation::Store;

	rg::RGRenderPassDefinition renderPassDef(math::Vector2i::Zero(), resolution);
	renderPassDef.SetDepthRenderTarget(depthRTDef);

	graphBuilder.FullScreenPass(RG_DEBUG_NAME("Render Terrain Material Cache Depth"),
								renderPassDef,
								RenderCacheDepthTexturePSO::pso,
								rg::EmptyDescriptorSets(),
								shaderConstants);

	return cacheDepth;
}


BEGIN_SHADER_STRUCT(RenderTerrainMaterialCachePermutationDomain)
	SHADER_STRUCT_FIELD(Bool, RENDER_POM_DEPTH)
END_SHADER_STRUCT();


GRAPHICS_PSO(RenderTerrainMaterialCachePSO)
{
	FULLSCREEN_VS();
	FRAGMENT_SHADER("Sculptor/Terrain/RenderTerrainMaterialCache.hlsl", RenderTerrainMaterialCacheFS);

	PERMUTATION_DOMAIN(RenderTerrainMaterialCachePermutationDomain)

	PRESET(lodsPomDepth);
	PRESET(lodsNoPomDepth);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		rhi::GraphicsPipelineDefinition psoDef;
		psoDef.rasterizationDefinition.cullMode = rhi::ECullMode::None;
		psoDef.renderTargetsDefinition.depthRTDefinition = rhi::DepthRenderTargetDefinition(rhi::EFragmentFormat::D16_UN_Float, rhi::ECompareOp::Less);

		// Base Color + Metallic
		psoDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(
			rhi::ColorRenderTargetDefinition
			{
				.format         = rhi::EFragmentFormat::RGBA8_UN_Float,
				.colorBlendType = rhi::ERenderTargetBlendType::Disabled,
				.alphaBlendType = rhi::ERenderTargetBlendType::Disabled,
			});

		// Normals
		psoDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(
			rhi::ColorRenderTargetDefinition
			{
				.format         = rhi::EFragmentFormat::RG16_UN_Float,
				.colorBlendType = rhi::ERenderTargetBlendType::Disabled,
				.alphaBlendType = rhi::ERenderTargetBlendType::Disabled,
			});

		// Roughness + Occlusion
		psoDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(
			rhi::ColorRenderTargetDefinition
			{
				.format         = rhi::EFragmentFormat::RG8_UN_Float,
				.colorBlendType = rhi::ERenderTargetBlendType::Disabled,
				.alphaBlendType = rhi::ERenderTargetBlendType::Disabled,
			});

		lodsNoPomDepth = CompilePermutation(compiler, psoDef, RenderTerrainMaterialCachePermutationDomain{ .RENDER_POM_DEPTH = false });

		psoDef.renderTargetsDefinition.colorRTsDefinition.emplace_back(
			rhi::ColorRenderTargetDefinition
			{
				.format         = rhi::EFragmentFormat::RG8_UN_Float,
				.colorBlendType = rhi::ERenderTargetBlendType::Disabled,
				.alphaBlendType = rhi::ERenderTargetBlendType::Disabled,
			});

		lodsPomDepth = CompilePermutation(compiler, psoDef, RenderTerrainMaterialCachePermutationDomain{ .RENDER_POM_DEPTH = true });
	}
};


BEGIN_SHADER_STRUCT(UpdateMaterialCacheConstants)
	SHADER_STRUCT_FIELD(TerrainMaterialData, terrainMaterial)
	SHADER_STRUCT_FIELD(math::Vector2f, minBounds)
	SHADER_STRUCT_FIELD(math::Vector2f, range)
END_SHADER_STRUCT();


void UpdateMaterialCache(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const UpdateMaterialCacheParams& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.lod->baseColorMetallic->GetResolution2D();

	const Bool hasPomDepth = params.lod->pomDepth != nullptr;

	rg::RGRenderPassDefinition renderPass(math::Vector2i::Zero(), resolution);
	renderPass.AddColorRenderTarget(
		rg::RGRenderTargetDef
		{
			.textureView    = graphBuilder.AcquireExternalTextureView(params.lod->baseColorMetallic),
			.loadOperation  = rhi::ERTLoadOperation::Load,
			.storeOperation = rhi::ERTStoreOperation::Store,
		});
	renderPass.AddColorRenderTarget(
		rg::RGRenderTargetDef
		{
			.textureView    = graphBuilder.AcquireExternalTextureView(params.lod->normals),
			.loadOperation  = rhi::ERTLoadOperation::Load,
			.storeOperation = rhi::ERTStoreOperation::Store,
		});
	renderPass.AddColorRenderTarget(
		rg::RGRenderTargetDef
		{
			.textureView    = graphBuilder.AcquireExternalTextureView(params.lod->roughnessOcclusion),
			.loadOperation  = rhi::ERTLoadOperation::Load,
			.storeOperation = rhi::ERTStoreOperation::Store,
		});

	if (hasPomDepth)
	{
		renderPass.AddColorRenderTarget(
			rg::RGRenderTargetDef
			{
				.textureView    = graphBuilder.AcquireExternalTextureView(params.lod->pomDepth),
				.loadOperation  = rhi::ERTLoadOperation::Load,
				.storeOperation = rhi::ERTStoreOperation::Store,
			});
	}

	const rg::RGTextureViewHandle depthTexture = RenderCacheDepthTexture(graphBuilder, rendererInterface, renderScene, params);

	renderPass.SetDepthRenderTarget(
		rg::RGRenderTargetDef
		{
			.textureView    = depthTexture,
			.loadOperation  = rhi::ERTLoadOperation::Load,
			.storeOperation = rhi::ERTStoreOperation::Store,
		});

	const rsc::TerrainDefinition& terrainDef = renderScene.GetTerrainDefinition();

	UpdateMaterialCacheConstants shaderConstants;
	shaderConstants.terrainMaterial = terrainDef.material;
	shaderConstants.minBounds = params.lod->minBounds;
	shaderConstants.range     = params.lod->maxBounds - params.lod->minBounds;

	graphBuilder.FullScreenPass(RG_DEBUG_NAME("Update Terrain Material Cache"),
								renderPass,
								hasPomDepth ? RenderTerrainMaterialCachePSO::lodsPomDepth : RenderTerrainMaterialCachePSO::lodsNoPomDepth,
								rg::EmptyDescriptorSets(),
								shaderConstants);
}

} // material_cache

} // namespace terrain_renderer


namespace lod
{

static constexpr Uint32 s_lodsNum = 6u;
static constexpr Uint32 s_maxLOD = s_lodsNum - 1u;
static constexpr Uint32 s_maxCachedBLASesPerLOD = 16u;
static constexpr Uint32 s_transactionsToLODBudgetPerFrame = 16u;


using BLASesCache = lib::InlineDynamicArray<lib::SharedPtr<rdr::BottomLevelAS>, s_maxCachedBLASesPerLOD>;


struct LODRTInfo
{
	lib::SharedPtr<rdr::Buffer> indexBuffer;
	Uint32 primitivesNum   = 0u;
	Uint32 verticesNum     = 0u;
	Uint32 verticesPerEdge = 0u;
};


struct TerrainGeometryCache
{
	lib::StaticArray<BLASesCache, s_lodsNum> blasCache;
	lib::StaticArray<LODRTInfo, s_lodsNum> lodsRTInfo;
};


struct TileState
{
	Uint8 lod = idxNone<Uint8>;
};


enum class ELODChangeMask
{
	None  = 0u,
	North = BIT(0u),
	East  = BIT(1u),
	South = BIT(2u),
	West  = BIT(3u)
};


struct LODState
{
	TerrainGeometryCache geometryCache;

	lib:: Span<TileState> tiles;
	lib:: Span<RayTracingGeometryDefinition> rtTiles;

	math::Vector2u tileRes = math::Vector2u(64u, 64u);

	Uint32 CoordToIdx(math::Vector2i coord) const
	{
		return coord.y() * tileRes.x() + coord.x();
	}
};


void InitializeLODState(LODState& state, lib::MemoryArena& arena, RenderScene& renderScene, math::Vector2u tileRes)
{
	SPT_PROFILER_FUNCTION();

	const Uint32 tilesNum = tileRes.x() * tileRes.y();
	state.tiles = arena.AllocateSpanUninitialized<TileState>(tilesNum);
	state.rtTiles = arena.AllocateSpanUninitialized<RayTracingGeometryDefinition>(tilesNum);

	for (TileState& tile : state.tiles)
	{
		tile.lod = idxNone<Uint8>;
	}

	for (RayTracingGeometryDefinition& rtTile : state.rtTiles)
	{
		new (&rtTile) RayTracingGeometryDefinition();
	}

	for (Uint8 lod = 0u; lod < s_lodsNum; ++lod)
	{
		LODRTInfo& lodRT = state.geometryCache.lodsRTInfo[lod];

		const Uint32 verticesPerEdge = utils::MeshletsNumToVerticesPerEdge(1u << (s_maxLOD - lod));

		lib::DynamicArray<rdr::HLSLStorage<Uint32>> indices = utils::BuildTileIndices(verticesPerEdge);
		rhi::BufferDefinition indexBufferDef;
		indexBufferDef.size   = std::max<Uint64>(1u, static_cast<Uint64>(indices.size())) * sizeof(rdr::HLSLStorage<Uint32>);
		indexBufferDef.usage  = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::ASBuildInputReadOnly);
		lodRT.indexBuffer     = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Terrain LOD Index Buffer"), indexBufferDef, rhi::EMemoryUsage::GPUOnly);
		lodRT.primitivesNum   = static_cast<Uint32>(indices.size()) / 3u;
		lodRT.verticesNum     = verticesPerEdge * verticesPerEdge;
		lodRT.verticesPerEdge = verticesPerEdge;

		rdr::GPUApi::GetTransfersManager().EnqueueUpload(lib::Ref(lodRT.indexBuffer), 0u, reinterpret_cast<const Byte*>(indices.data()), indexBufferDef.size);
	}
}

void DestroyLODState(LODState& state)
{
	for (RayTracingGeometryDefinition& rtTile : state.rtTiles)
	{
		rtTile.~RayTracingGeometryDefinition();
	}
}


Uint32 TileDistance(const math::Vector2i& a, const math::Vector2i& b)
{
	return std::max(std::abs(a.x() - b.x()), std::abs(a.y() - b.y()));
}


lib::SharedPtr<rdr::BottomLevelAS> GetOrCreateBLASForLOD(TerrainGeometryCache& geoCache, Uint8 lod)
{
	if (geoCache.blasCache[lod].empty())
	{
		const LODRTInfo& lodRT = geoCache.lodsRTInfo[lod];

		rhi::BLASDefinition blasDef;
		blasDef.geometryFlags = rhi::EGeometryFlags::Opaque;
		blasDef.trianglesGeometry.maxPrimitivesNum = lodRT.primitivesNum;
		blasDef.trianglesGeometry.maxVerticesNum   = lodRT.verticesNum;

		return rdr::ResourcesManager::CreateBLAS(RENDERER_RESOURCE_NAME("Terrain BLAS"), blasDef);
	}

	lib::SharedPtr<rdr::BottomLevelAS> blas = std::move(geoCache.blasCache[lod].Back());
	geoCache.blasCache[lod].PopBack();
	return blas;
}


void ReturnBLASToCache(TerrainGeometryCache& geoCache, Uint8 lod, lib::SharedPtr<rdr::BottomLevelAS>&& blas)
{
	SPT_CHECK(blas);
	SPT_CHECK(lod < s_lodsNum);

	if (!geoCache.blasCache[lod].IsMaxCapacity())
	{
		geoCache.blasCache[lod].PushBack(std::move(blas));
	}
}


struct LODTransaction
{
	math::Vector2i tile = math::Vector2i::Zero();
	Uint8 fromLOD       = 0u;
	Uint8 toLOD         = 0u;
};


class LODTransactions
{
public:

	explicit LODTransactions(lib::MemoryArena& arena)
		: m_transactions(arena)
	{
		for (Int32 i = 0; i < s_lodsNum; ++i)
		{
			m_targetLODTransactionsBudget[i] = s_transactionsToLODBudgetPerFrame;
		}
	}

	const lib::DynamicPushArray<LODTransaction>& GetTransactions() const { return m_transactions; }

	void AddTransaction(const LODTransaction& transaction)
	{
		SPT_CHECK(transaction.toLOD < s_lodsNum);
		SPT_CHECK(m_targetLODTransactionsBudget[transaction.toLOD] > 0);

		m_transactions.EmplaceBack(transaction);
		--m_targetLODTransactionsBudget[transaction.toLOD];
	}

	Int32 GetRemainingBudgetForLOD(Uint8 lod) const
	{
		SPT_CHECK(lod < s_lodsNum);
		return m_targetLODTransactionsBudget[lod];
	}

private:

	lib::StaticArray<Int32, s_lodsNum> m_targetLODTransactionsBudget;

	lib::DynamicPushArray<LODTransaction> m_transactions;
};


static Bool CanSetLOD(const LODState& state, math::Vector2i tileCoord, Uint8 desiredLOD)
{
	const math::Vector2i neighbourhoodStart = (tileCoord - math::Vector2i::Constant(1)).cwiseMax(math::Vector2i::Zero());
	const math::Vector2i neighbourhoodEnd = (tileCoord + math::Vector2i::Constant(1)).cwiseMin(state.tileRes.cast<Int32>() - math::Vector2i::Constant(1));

	// Difference between neighbouring tiles LODs can't be greater than 1
	for (Int32 tileY = neighbourhoodStart.y(); tileY <= neighbourhoodEnd.y(); ++tileY)
	{
		for (Int32 tileX = neighbourhoodStart.x(); tileX <= neighbourhoodEnd.x(); ++tileX)
		{
			const math::Vector2i neighbourCoord = math::Vector2i(tileX, tileY);
			if (neighbourCoord == tileCoord)
			{
				continue;
			}

			const Uint8 neighbourLOD = state.tiles[state.CoordToIdx(neighbourCoord)].lod;
			if (std::abs(static_cast<Int32>(desiredLOD) - static_cast<Int32>(neighbourLOD)) > 1)
			{
				return false;
			}
		}
	}

	return true;
}


static void UpdateLODState(LODState& state, math::Vector2i cameraTileCoord, LODTransactions& outTransactions)
{
	SPT_PROFILER_FUNCTION();

	for (Uint32 tileY = 0u; tileY < state.tileRes.y(); ++tileY)
	{
		for (Uint32 tileX = 0u; tileX < state.tileRes.x(); ++tileX)
		{
			const math::Vector2i tileCoord = math::Vector2i(tileX, tileY);
			const Uint32 distance = TileDistance(tileCoord, cameraTileCoord);

			const Uint8 currentLOD = state.tiles[state.CoordToIdx(tileCoord)].lod;
			const Uint8 desiredLOD = static_cast<Uint8>(std::min(std::max<Uint32>(distance, 1u) - 1u, s_maxLOD));

			Uint8 targetLOD = currentLOD;
			if (desiredLOD < currentLOD)
			{
				targetLOD = currentLOD - 1u;
			}
			else if (desiredLOD > currentLOD)
			{
				targetLOD = currentLOD + 1u;
			}
			targetLOD = std::min<Uint8>(targetLOD, s_maxLOD);

			if (currentLOD != targetLOD)
			{
				if (currentLOD == idxNone<Uint8> || CanSetLOD(state, tileCoord, targetLOD))
				{
					if (outTransactions.GetRemainingBudgetForLOD(targetLOD) > 0)
					{
						outTransactions.AddTransaction(LODTransaction{
							.tile    = tileCoord,
							.fromLOD = currentLOD,
							.toLOD   = targetLOD
						});

						state.tiles[state.CoordToIdx(tileCoord)].lod = targetLOD;
					}
				}
			}
		}
	}
}


SIMPLE_COMPUTE_PSO(TerrainBuildTileVertexBufferPSO, "Sculptor/Terrain/TerrainBuildTilesVertices.hlsl", BuildTileVertexBufferCS);


BEGIN_SHADER_STRUCT(TerrainBuildTilesVerticesConstants)
	SHADER_STRUCT_FIELD(gfx::RWTypedBufferRef<math::Vector3f>, rwVertexBuffer)
	SHADER_STRUCT_FIELD(Uint32,                                tileIdx)
	SHADER_STRUCT_FIELD(Uint32,                                verticesPerEdge)
	SHADER_STRUCT_FIELD(Real32,                                verticesSpacing)
END_SHADER_STRUCT();


void BuildTileVertexBuffer(rg::RenderGraphBuilder& graphBuilder, Uint32 tileIdx, const LODRTInfo& lod, rg::RGBufferViewHandle vertexBuffer)
{
	TerrainBuildTilesVerticesConstants shaderConstants;
	shaderConstants.rwVertexBuffer  = vertexBuffer;
	shaderConstants.tileIdx         = tileIdx;
	shaderConstants.verticesPerEdge = lod.verticesPerEdge;
	shaderConstants.verticesSpacing = terrain_consts::tileSizeMeters / static_cast<Real32>(lod.verticesPerEdge - 1u);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Build Terrain Runtime Vertices"),
						  TerrainBuildTileVertexBufferPSO::pso,
						  math::Utils::DivideCeil(lod.verticesNum, 64u),
						  rg::EmptyDescriptorSets(),
						  shaderConstants);
}


static void ProcessLODTransactions(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, LODState& state, const LODTransactions& transactions)
{
	SPT_PROFILER_FUNCTION();

	if (transactions.GetTransactions().IsEmpty())
	{
		return;
	}

	for (const LODTransaction& transaction : transactions.GetTransactions())
	{
		const Uint32 tileIdx = state.CoordToIdx(transaction.tile);

		lib::SharedPtr<rdr::BottomLevelAS> blas = std::move(state.rtTiles[tileIdx].blas);

		if (blas)
		{
			ReturnBLASToCache(state.geometryCache, transaction.fromLOD, std::move(blas));
		}
	}

	const Uint32 tilesNum = transactions.GetTransactions().GetSize();

	lib::MemoryArena& memArena = rendererInterface.GetSceneRendererFrameArena();

	lib::ManagedSpan<rg::BLASBuildCommand> buildCommands = memArena.AllocateArray<rg::BLASBuildCommand>(tilesNum);

	Uint32 scratchSize = 0u;
	Uint32 buildCommandIdx = 0u;

	for (const LODTransaction& transaction : transactions.GetTransactions())
	{
		const Uint32 tileIdx = state.CoordToIdx(transaction.tile);
		RayTracingGeometryDefinition& rtTile = state.rtTiles[tileIdx];

		rtTile.blas = GetOrCreateBLASForLOD(state.geometryCache, transaction.toLOD);

		const LODRTInfo& rtInfo = state.geometryCache.lodsRTInfo[transaction.toLOD];

		const rg::RGBufferViewHandle indexBuffer = graphBuilder.AcquireExternalBufferView(rtInfo.indexBuffer->GetFullView());

		const rg::RGBufferViewHandle vertexBuffer = graphBuilder.CreateStorageBufferView(RG_DEBUG_NAME("Terrain Tile Vertex Buffer"), sizeof(rdr::HLSLStorage<math::Vector3f>) * rtInfo.verticesNum);

		const Uint32 scratchOffset = scratchSize;
		scratchSize += static_cast<Uint32>(rtTile.blas->GetRHI().GetBuildScratchSize());

		BuildTileVertexBuffer(graphBuilder, tileIdx, rtInfo, vertexBuffer );

		rg::BLASBuildCommand& buildCommand = buildCommands[buildCommandIdx++];
		buildCommand.blas                = rtTile.blas;
		buildCommand.vertexBufferView    = vertexBuffer;
		buildCommand.indexBufferView     = indexBuffer;
		buildCommand.primitivesNum       = rtInfo.primitivesNum;
		buildCommand.scratchBufferOffset = scratchOffset;
	}

	rhi::BufferDefinition scratchBufferDef;
	scratchBufferDef.size  = scratchSize;
	scratchBufferDef.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::ASBuildInputReadOnly);
	const rg::RGBufferViewHandle scratchBuffer = graphBuilder.CreateBufferView(RG_DEBUG_NAME("Terrain BLAS Build Scratch Buffer"), scratchBufferDef, rhi::EMemoryUsage::GPUOnly);

	for (rg::BLASBuildCommand& buildCommand : buildCommands)
	{
		buildCommand.scratchBufferView = scratchBuffer;
	}

	graphBuilder.BuildBLASes(RG_DEBUG_NAME("Build Terrain BLASes"), buildCommands);
}

void WriteTilesLODState(const lib::SharedPtr<rdr::Buffer>& buffer, const LODState& state)
{
	SPT_PROFILER_FUNCTION();

	rhi::RHIMappedBuffer<Uint16> mappedBuffer(buffer->GetRHI());
	
	for (Uint32 tileX = 0u; tileX < state.tileRes.x(); ++tileX)
	{
		for (Uint32 tileY = 0u; tileY < state.tileRes.y(); ++tileY)
		{
			const math::Vector2i tileCoord = math::Vector2i(tileX, tileY);
			const Uint32 tileIdx = state.CoordToIdx(tileCoord);

			const Uint8 lod = state.tiles[tileIdx].lod;

			ELODChangeMask changeMask = ELODChangeMask::None;
			if (tileCoord.y() > 0)
			{
				if (state.tiles[state.CoordToIdx(tileCoord - math::Vector2i(0, 1))].lod > lod)
				{
					changeMask = lib::Flags(changeMask, ELODChangeMask::North);
				}
			}

			if (tileCoord.x() < static_cast<Int32>(state.tileRes.x() - 1))
			{
				if (state.tiles[state.CoordToIdx(tileCoord + math::Vector2i(1, 0))].lod > lod)
				{
					changeMask = lib::Flags(changeMask, ELODChangeMask::East);
				}
			}

			if (tileCoord.y() < static_cast<Int32>(state.tileRes.y() - 1))
			{
				if (state.tiles[state.CoordToIdx(tileCoord + math::Vector2i(0, 1))].lod > lod)
				{
					changeMask = lib::Flags(changeMask, ELODChangeMask::South);
				}
			}

			if (tileCoord.x() > 0)
			{
				if (state.tiles[state.CoordToIdx(tileCoord - math::Vector2i(1, 0))].lod > lod)
				{
					changeMask = lib::Flags(changeMask, ELODChangeMask::West);
				}
			}

			const Uint16 value = (static_cast<Uint16>(lod) & 0x00FF) | (static_cast<Uint16>(changeMask) << 8);
			mappedBuffer[tileIdx] = value;
		}
	}
}

} // lod

struct TerrainRenderInstance
{
	terrain_renderer::material_cache::MaterialCacheState materialCacheState;

	terrain_renderer::material_cache::UpdateMaterialCacheParams materialCacheUpdateParams;

	lod::LODState lodState;
	lod::LODTransactions* lodTransactions = nullptr;
};


lib::Span<const RayTracingGeometryDefinition> TerrainRTGeometryProvider::GetRayTracingGeometries() const
{
	return renderer_params::enableTerrain && instance ? instance->lodState.rtTiles : lib::Span<const RayTracingGeometryDefinition>();
}


BEGIN_SHADER_STRUCT(TerrainSampleMaterialData)
	SHADER_STRUCT_FIELD(Uint32, unused)
END_SHADER_STRUCT();


TerrainRenderSystem::TerrainRenderSystem(lib::MemoryArena& arena, RenderScene& owningScene)
	: Super(arena, owningScene)
{
	m_supportedStages = ERenderStage::VisibilityBuffer;
}

void TerrainRenderSystem::Initialize(lib::MemoryArena& arena, RenderScene& renderScene)
{
	Super::Initialize(arena, renderScene);

	SPT_CHECK(!m_initialized)

	const math::Vector2u tilesRes = utils::ComputeTilesResolution();

	const lib::DynamicArray<rdr::HLSLStorage<TerrainClipmapTileGPU>> clipmapTiles = utils::BuildInitialClipmapTiles();
	const rhi::BufferDefinition clipmapTilesDef(std::max<Uint64>(1u, static_cast<Uint64>(clipmapTiles.size())) * sizeof(rdr::HLSLStorage<TerrainClipmapTileGPU>), rhi::EBufferUsage::Storage);
	m_tilesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Terrain Clipmap Tiles"), clipmapTilesDef, rhi::EMemoryUsage::CPUToGPU);
	rdr::UploadDataToBuffer(lib::Ref(m_tilesBuffer), 0u, reinterpret_cast<const Byte*>(clipmapTiles.data()), static_cast<Uint64>(clipmapTiles.size()) * sizeof(rdr::HLSLStorage<TerrainClipmapTileGPU>));

	const lib::DynamicArray<rdr::HLSLStorage<math::Vector2f>> meshletVertices = utils::BuildMeshletVertices();
	const rhi::BufferDefinition meshletVerticesDef(std::max<Uint64>(1u, static_cast<Uint64>(meshletVertices.size())) * sizeof(rdr::HLSLStorage<math::Vector2f>), rhi::EBufferUsage::Storage);
	m_meshletVerticesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Terrain Meshlet Vertices"), meshletVerticesDef, rhi::EMemoryUsage::CPUToGPU);
	rdr::UploadDataToBuffer(lib::Ref(m_meshletVerticesBuffer), 0u, reinterpret_cast<const Byte*>(meshletVertices.data()), static_cast<Uint64>(meshletVertices.size()) * sizeof(rdr::HLSLStorage<math::Vector2f>));

	const lib::DynamicArray<rdr::HLSLStorage<Uint32>> meshletIndices = utils::BuildMeshletIndices();
	const rhi::BufferDefinition meshletIndicesDef(std::max<Uint64>(1u, static_cast<Uint64>(meshletIndices.size())) * sizeof(rdr::HLSLStorage<Uint32>), lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::ASBuildInputReadOnly));
	m_meshletIndicesBuffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Terrain Meshlet Indices"), meshletIndicesDef, rhi::EMemoryUsage::CPUToGPU);
	rdr::UploadDataToBuffer(lib::Ref(m_meshletIndicesBuffer), 0u, reinterpret_cast<const Byte*>(meshletIndices.data()), static_cast<Uint64>(meshletIndices.size()) * sizeof(rdr::HLSLStorage<Uint32>));

	TerrainSampleMaterialData materialData{};

	mat::MaterialDefinition materialDefinition;
	materialDefinition.name          = "Terrain Material";
	materialDefinition.customOpacity = false;
	materialDefinition.transparent   = false;
	materialDefinition.doubleSided   = true;
	materialDefinition.emissive      = false;

	ecs::EntityHandle material = mat::MaterialsSubsystem::Get().CreateMaterial(materialDefinition, materialData);
	const mat::MaterialProxyComponent& materialProxy = material.get<mat::MaterialProxyComponent>();
	m_terrainMaterialShader = materialProxy.params.shader;

	m_renderInstance = arena.AllocateType<TerrainRenderInstance>();
	terrain_renderer::material_cache::InitializeMaterialCache(m_renderInstance->materialCacheState);
	lod::InitializeLODState(m_renderInstance->lodState, arena, renderScene, tilesRes);

	{
		m_rtGeometryProvider.instance = m_renderInstance;

		lib::DynamicArray<ecs::EntityHandle> materialSlotsArray;
		materialSlotsArray.resize(clipmapTiles.size(), material);

		MaterialSlotsChunkHandle materialSlots = renderScene.materials.CreateMaterialSlotsChain(materialSlotsArray);

		RTInstance rtTerrainInstance
		{
			.type          = ERTInstanceType::Terrain,
			.rtGeometry    = m_rtGeometryProvider,
			.materialSlots = materialSlots
		};

		m_rtInstanceHandle = renderScene.rt.instances.Add(rtTerrainInstance);
	}

	for (Uint32 i = 0u; i < m_tileLODsBuffers.size(); ++i)
	{
		rhi::BufferDefinition tileLODBufferDef;
		tileLODBufferDef.size  = clipmapTiles.size() * sizeof(Uint16);
		tileLODBufferDef.usage = rhi::EBufferUsage::Storage;
		m_tileLODsBuffers[i] = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Terrain Tile LODs Buffer"), tileLODBufferDef, rhi::EMemoryUsage::CPUToGPU);
	}

	m_initialized = true;
}

void TerrainRenderSystem::Deinitialize(RenderScene& renderScene)
{
	Super::Deinitialize(renderScene);

	lod::DestroyLODState(m_renderInstance->lodState);

	m_tilesBuffer.reset();
	m_meshletVerticesBuffer.reset();
	m_meshletIndicesBuffer.reset();

	m_renderInstance->~TerrainRenderInstance();
	m_renderInstance = nullptr;

	m_tileLODsBuffers.fill(nullptr);

	m_initialized = false;
}

void TerrainRenderSystem::Update(const SceneUpdateContext& context)
{
	Super::Update(context);

	if (!IsEnabled())
	{
		return;
	}

	const math::Vector2f cameraLocation = context.mainRenderView.GetLocation().head<2>();

	const Uint32 currentLOD = context.rendererSettings.frame.GetFrameIdx() % terrain_consts::materialCacheLODsNum;

	terrain_renderer::material_cache::UpdateMaterialCacheParams& updateCacheParams = m_renderInstance->materialCacheUpdateParams;
	updateCacheParams.lod       = &m_renderInstance->materialCacheState.lods[currentLOD];
	updateCacheParams.prevMinBounds = updateCacheParams.lod->minBounds;
	updateCacheParams.prevMaxBounds = updateCacheParams.lod->maxBounds;

	const math::Vector2f lodExtent = math::Vector2f::Constant(updateCacheParams.lod->sizeMeters * 0.5f);

	updateCacheParams.lod->minBounds = cameraLocation - lodExtent;
	updateCacheParams.lod->maxBounds = cameraLocation + lodExtent;

	const math::Vector2i cameraTileCoord = ((cameraLocation + math::Vector2f::Constant(terrain_consts::clipmapExtentMeters)) / terrain_consts::tileSizeMeters).cast<Int32>();

	lib::MemoryArena& memArena = context.rendererInterface.GetSceneRendererFrameArena();
	lod::LODTransactions* lodTransactions = memArena.AllocateType<lod::LODTransactions>(memArena);
	lod::UpdateLODState(m_renderInstance->lodState, cameraTileCoord, *lodTransactions);
	m_renderInstance->lodTransactions = lodTransactions;
}

void TerrainRenderSystem::UpdateGPUSceneData(RenderSceneConstants& sceneData)
{
	SPT_CHECK(m_initialized);

	const Uint64 frameIdx = GetOwningScene().GetCurrentFrameRef().GetFrameIdx();

	lib::SharedPtr<rdr::Buffer>& currentFrameTileLODsBuffer = m_tileLODsBuffers[frameIdx % m_tileLODsBuffers.size()];

	const TerrainDefinition& terrainDef = GetOwningScene().GetTerrainDefinition();
	const lib::SharedPtr<rdr::TextureView>& heightMap = terrainDef.heightMap;

	TerrainHeightMap heightMapData;
	heightMapData.texture        = heightMap;
	heightMapData.res            = heightMap ? heightMap->GetResolution2D() : math::Vector2u{0u, 0u};
	heightMapData.invRes         = heightMap ? math::Vector2f::Ones().cwiseQuotient(heightMapData.res.cast<Real32>()) : math::Vector2f{0.f, 0.f};
	heightMapData.spanMeters     = math::Vector2f::Constant(terrain_consts::clipmapExtentMeters * 2.f);
	heightMapData.invSpanMeters  = math::Vector2f::Ones().cwiseQuotient(heightMapData.spanMeters);
	heightMapData.metersPerTexel = heightMap ? heightMapData.spanMeters.cwiseQuotient(heightMapData.res.cast<Real32>()) : math::Vector2f{0.f, 0.f};
	heightMapData.minHeight      = 0.f;
	heightMapData.maxHeight      = 150.f;

	TerrrainMaterialCache matCache;
	for (Uint32 i = 0u; i < terrain_consts::materialCacheLODsNum; ++i)
	{
		const terrain_renderer::material_cache::MaterialCacheState::LOD& lodCache = m_renderInstance->materialCacheState.lods[i];

		matCache.lods[i].baseColorMetallic  = lodCache.baseColorMetallic;
		matCache.lods[i].normals            = lodCache.normals;
		matCache.lods[i].roughnessOcclusion = lodCache.roughnessOcclusion;
		matCache.lods[i].pomDepth           = lodCache.pomDepth;
		matCache.lods[i].minBounds          = lodCache.minBounds;
		matCache.lods[i].rcpRange           = (lodCache.maxBounds - lodCache.minBounds).cwiseInverse();
	}

	TerrainSceneData terrainData;
	terrainData.heightMap           = heightMapData;
	terrainData.farLODBaseColor     = terrainDef.farLODBaseColor;
	terrainData.farLODProps         = terrainDef.farLODProps;
	terrainData.farLODMinBounds     = terrainDef.farLODMinBounds;
	terrainData.farLODRcpBounds     = (terrainDef.farLODMaxBounds - terrainDef.farLODMinBounds).cwiseInverse();
	terrainData.tiles               = m_tilesBuffer->GetFullView();
	terrainData.tilesLODs           = currentFrameTileLODsBuffer->GetFullView();
	terrainData.tilesNum            = static_cast<Uint32>(m_tilesBuffer->GetSize() / sizeof(rdr::HLSLStorage<TerrainClipmapTileGPU>));
	terrainData.tileSizeMeters      = terrain_consts::tileSizeMeters;
	terrainData.clipmapExtentMeters = terrain_consts::clipmapExtentMeters;
	terrainData.meshletVertices     = m_meshletVerticesBuffer->GetFullView();
	terrainData.meshletIndices      = m_meshletIndicesBuffer->GetFullView();
	terrainData.meshletVerticesNum  = terrain_consts::meshletVerticesNum;
	terrainData.meshletIndicesNum   = terrain_consts::meshletIndicesNum;
	terrainData.meshletTranglesNum  = terrain_consts::meshletIndicesNum / 3u;
	terrainData.materialCache       = matCache;

	sceneData.terrain = terrainData;
}

void TerrainRenderSystem::RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	if (!IsEnabled())
	{
		return;
	}

	SPT_CHECK(!viewSpecs.IsEmpty());

	terrain_renderer::material_cache::UpdateMaterialCache(graphBuilder, rendererInterface, renderScene, m_renderInstance->materialCacheUpdateParams);

	ViewRenderingSpec* mainView = *viewSpecs.begin();
	SPT_CHECK(mainView != nullptr);

	lod::ProcessLODTransactions(graphBuilder, rendererInterface, m_renderInstance->lodState, *m_renderInstance->lodTransactions);
	m_renderInstance->lodTransactions = nullptr;

	const Uint64 frameIdx = GetOwningScene().GetCurrentFrameRef().GetFrameIdx();

	lib::SharedPtr<rdr::Buffer>& currentFrameTileLODsBuffer = m_tileLODsBuffers[frameIdx % m_tileLODsBuffers.size()];
	lod::WriteTilesLODState(currentFrameTileLODsBuffer, m_renderInstance->lodState);
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
