#pragma once

#include "SculptorCoreTypes.h"
#include "AssetTypes.h"
#include "RHICore//RHITextureTypes.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::as
{

struct TerrainAssetDefinition;


struct CompiledTerrainHeader
{
	struct TextureInfo
	{
		math::Vector2u       resolution = math::Vector2u::Zero();
		rhi::EFragmentFormat format     = rhi::EFragmentFormat::None;
		Uint32               dataOffset = 0u;
		Uint32               dataSize   = 0u;
	};

	TextureInfo heightMap{};

	TextureInfo farLODBaseColor;
	TextureInfo farLODProps;

	ResourcePathID       terrainMaterialAssetID = InvalidResourcePathID;
};


struct TerrainCompilationResult
{
	lib::DynamicArray<Byte> blob;
};


namespace terrain_compiler
{

std::optional<TerrainCompilationResult> CompileTerrain(const AssetInstance& asset, const TerrainAssetDefinition& definition);

} // terrain_compiler

} // spt::as
