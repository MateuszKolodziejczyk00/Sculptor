#include "TerrainCompiler.h"
#include "Bindless/BindlessTypes.h"
#include "Engine.h"
#include "MaterialsUnifiedData.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResourcesPool.h"
#include "TerrainAsset.h"
#include "Loaders/TextureLoader.h"
#include "Compression/TextureCompressor.h"
#include "AssetsSystem.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "Transfers/GPUDeferredCommandsQueue.h"
#include "Utils/TransfersUtils.h"


SPT_DEFINE_LOG_CATEGORY(TerrainCompiler, true);

namespace spt::as::terrain_compiler
{

namespace bake_far_lod
{

BEGIN_SHADER_STRUCT(BakeFarLODConstants)
	SHADER_STRUCT_FIELD(math::Vector2u,                       resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,                       minBounds)
	SHADER_STRUCT_FIELD(math::Vector2f,                       maxBounds)
	SHADER_STRUCT_FIELD(rsc::TerrainMaterialData,             terrainMaterial)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector3f>, rwFarLODBaseColor)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector3f>, rwFarLODProps)
	SHADER_STRUCT_FIELD(mat::MaterialUnifiedData,             materialsData)
END_SHADER_STRUCT()


SIMPLE_COMPUTE_PSO(BakeFarLODPSO, "Sculptor/Terrain/BakeFarLOD.hlsl", BakeFarLODCS)


void Execute(rg::RenderGraphBuilder& graphBuilder, const BakeFarLODConstants& constants)
{
	SPT_PROFILER_FUNCTION();

	graphBuilder.Dispatch(RG_DEBUG_NAME("Bake Far LOD"),
						  BakeFarLODPSO::pso,
						  math::Utils::DivideCeil(constants.resolution, math::Vector2u(16u, 16u)),
						  rg::EmptyDescriptorSets(),
						  constants);
}

} // bake_far_lod

std::optional<TerrainCompilationResult> CompileTerrain(const AssetInstance& asset, const TerrainAssetDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	AssetsSystem& assetSystem = asset.GetOwningSystem();

	TerrainCompilationResult result;

	CompiledTerrainHeader header{};
	result.blob.resize(sizeof(CompiledTerrainHeader));

	lib::MemoryArena tempArena("TerrainCompilationTempArena", 1024u * 1024u, 64u * 1024u * 1024u);

	if (!definition.heightMapTex.empty())
	{
		const lib::Path heightMapPath = asset.GetDirectoryPath() / definition.heightMapTex;
		const gfx::LoadedTextureData heightMapData = gfx::TextureLoader::LoadTextureData(heightMapPath.generic_string(), tempArena);

		if (!heightMapData.data.empty())
		{
			SPT_CHECK(heightMapData.resolution.x() % 4u == 0u && heightMapData.resolution.y() % 4u == 0u);
			SPT_CHECK(heightMapData.format == rhi::EFragmentFormat::R8_UN_Float || heightMapData.format == rhi::EFragmentFormat::RGBA8_UN_Float);

			const Uint64 sizeBC4 = gfx::compressor::ComputeCompressedSizeBC4(heightMapData.resolution.head<2>());
			lib::Span<Byte> heightBC4 = tempArena.AllocateSpanUninitialized<Byte>(sizeBC4);

			lib::Span<Byte> heightMapDataSpan(heightMapData.data.data(), heightMapData.data.size());
			if (heightMapData.format == rhi::EFragmentFormat::RGBA8_UN_Float)
			{
				for (Uint64 i = 0; i < heightMapData.data.size(); i += 4)
				{
					// Convert RGBA8_UN_Float to R8_UN_Float by taking the red channel and discarding the rest
					heightMapData.data[i / 4] = heightMapData.data[i];
				}

				heightMapDataSpan = lib::Span<Byte>(heightMapData.data.data(), heightMapData.data.size() / 4);
			}

			gfx::compressor::CompressSurfaceToBC4(heightBC4, gfx::compressor::Surface2D{ heightMapData.resolution.head<2>(), heightMapDataSpan });

			header.heightMap.resolution = heightMapData.resolution.head<2>();
			header.heightMap.format     = rhi::EFragmentFormat::BC4_UN;
			header.heightMap.dataOffset = static_cast<Uint32>(result.blob.size());
			header.heightMap.dataSize   = static_cast<Uint32>(heightBC4.size());

			result.blob.insert(result.blob.end(), heightBC4.begin(), heightBC4.end());
		}
		else
		{
			SPT_LOG_ERROR(TerrainCompiler, "Failed to load terrain height map from path: {}", heightMapPath.generic_string());
		}
	}

	if (!definition.terrainMaterial.empty())
	{
		const ResourcePath terrainMaterialPath = asset.ResolveAssetRelativePath(definition.terrainMaterial);

		if (terrainMaterialPath.IsValid())
		{
			if (!assetSystem.CompileAssetIfDeprecated(terrainMaterialPath))
			{
				SPT_LOG_ERROR(TerrainCompiler, "Failed to compile terrain material asset with path: {}", definition.terrainMaterial.generic_string());
			}
			else
			{
				header.terrainMaterialAssetID = terrainMaterialPath.GetID();
			}
		}

		if (header.terrainMaterialAssetID != InvalidResourcePathID)
		{
			TerrainMaterialAssetHandle terrainMaterial = assetSystem.LoadAndInitAssetChecked<TerrainMaterialAsset>(terrainMaterialPath);

			// Flush terrain material uploads
			gfx::GPUDeferredCommandsQueue& queue = engn::GetEngine().GetPluginsManager().GetPluginChecked<gfx::GPUDeferredCommandsQueue>();
			queue.ForceFlushCommands();
			rdr::FlushPendingUploads();

			rg::RenderGraphResourcesPool renderResourcesPool;
			rg::RenderGraphBuilder graphBuilder(tempArena, renderResourcesPool);

			SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Terrain Compilation (GPU)");

			const math::Vector2u farLODResolution = math::Vector2u(2048u, 2048u);

			const rg::RGTextureViewHandle farLODBaseColor = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Far LOD Base Color"), rg::TextureDef(farLODResolution, rhi::EFragmentFormat::RGBA8_UN_Float));
			const rg::RGTextureViewHandle farLODProps     = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Far LOD Props"), rg::TextureDef(farLODResolution, rhi::EFragmentFormat::RGBA8_UN_Float));

			bake_far_lod::BakeFarLODConstants bakeConstants{};
			bakeConstants.resolution        = farLODResolution;
			bakeConstants.minBounds         = math::Vector2f(-1024.f, -1024.f);
			bakeConstants.maxBounds         = math::Vector2f(1024.f, 1024.f);
			bakeConstants.terrainMaterial   = terrainMaterial->GetTerrainMaterialData();
			bakeConstants.rwFarLODBaseColor = farLODBaseColor;
			bakeConstants.rwFarLODProps     = farLODProps;
			bakeConstants.materialsData     = mat::MaterialsUnifiedData::Get().GetMaterialUnifiedData();

			bake_far_lod::Execute(graphBuilder, bakeConstants);

			const lib::SharedRef<rdr::Buffer> farLODBaseColorData = graphBuilder.DownloadTextureToBuffer(RG_DEBUG_NAME("Download Far LOD Base Color"), farLODBaseColor);
			const lib::SharedRef<rdr::Buffer> farLODPropsData = graphBuilder.DownloadTextureToBuffer(RG_DEBUG_NAME("Download Far LOD Props"), farLODProps);

			graphBuilder.Execute();
			rdr::GPUApi::WaitIdle();

			{
				const rhi::RHIMappedByteBuffer farLODBaseColorMappedData(farLODBaseColorData->GetRHI());

				const Uint64 sizeBC1 = gfx::compressor::ComputeCompressedSizeBC1(farLODResolution);
				lib::Span<Byte> farLODBC1 = tempArena.AllocateSpanUninitialized<Byte>(sizeBC1);
				gfx::compressor::CompressSurfaceToBC1(farLODBC1, gfx::compressor::Surface2D{ farLODResolution, farLODBaseColorMappedData.GetSpan() });

				header.farLODBaseColor.resolution = farLODResolution;
				header.farLODBaseColor.format     = rhi::EFragmentFormat::BC1_UN;
				header.farLODBaseColor.dataOffset = static_cast<Uint32>(result.blob.size());
				header.farLODBaseColor.dataSize   = static_cast<Uint32>(sizeBC1);

				result.blob.insert(result.blob.end(), farLODBC1.begin(), farLODBC1.end());
			}

			{
				const rhi::RHIMappedByteBuffer farLODPropsMappedData(farLODPropsData->GetRHI());

				const Uint64 sizeBC1 = gfx::compressor::ComputeCompressedSizeBC1(farLODResolution);
				lib::Span<Byte> farLODOC4 = tempArena.AllocateSpanUninitialized<Byte>(sizeBC1);
				gfx::compressor::CompressSurfaceToBC1(farLODOC4, gfx::compressor::Surface2D{ farLODResolution, farLODPropsMappedData.GetSpan() });

				header.farLODProps.resolution = farLODResolution;
				header.farLODProps.format     = rhi::EFragmentFormat::BC1_UN;
				header.farLODProps.dataOffset = static_cast<Uint32>(result.blob.size());
				header.farLODProps.dataSize   = static_cast<Uint32>(sizeBC1);

				result.blob.insert(result.blob.end(), farLODOC4.begin(), farLODOC4.end());
			}
		}
	}

	std::memcpy(result.blob.data(), &header, sizeof(header));

	return result;
}

} // spt::as::terrain_compiler
