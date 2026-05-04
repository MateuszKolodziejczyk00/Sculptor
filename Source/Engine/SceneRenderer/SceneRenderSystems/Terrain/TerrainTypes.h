#pragma once

#include "RenderSceneMacros.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Bindless/BindlessTypes.h"


namespace spt::rsc
{

namespace terrain_consts
{
static constexpr Uint32 materialCacheLODsNum = 6u;
} // namespace terrain_consts


BEGIN_SHADER_STRUCT(TerrainClipmapTileGPU)
	SHADER_STRUCT_FIELD(Int32, tileCoordX)
	SHADER_STRUCT_FIELD(Int32, tileCoordY)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(TerrainHeightMap)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<Real32>, texture)
	SHADER_STRUCT_FIELD(math::Vector2u,                 res)
	SHADER_STRUCT_FIELD(math::Vector2f,                 invRes)
	SHADER_STRUCT_FIELD(math::Vector2f,                 spanMeters)
	SHADER_STRUCT_FIELD(math::Vector2f,                 invSpanMeters)
	SHADER_STRUCT_FIELD(math::Vector2f,                 metersPerTexel)
	SHADER_STRUCT_FIELD(Real32,                         minHeight)
	SHADER_STRUCT_FIELD(Real32,                         maxHeight)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(TerrrainMaterialCacheLOD)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector4f>, baseColorMetallic)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector2f>, normals)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector2f>, roughnessOcclusion)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<Real32>,         depth)
	SHADER_STRUCT_FIELD(math::Vector2f,                         minBounds)
	SHADER_STRUCT_FIELD(math::Vector2f,                         rcpRange)
END_SHADER_STRUCT();


using TerrrainMaterialCacheLODs = lib::StaticArray<TerrrainMaterialCacheLOD, terrain_consts::materialCacheLODsNum>;


BEGIN_SHADER_STRUCT(TerrrainMaterialCache)
	SHADER_STRUCT_FIELD(TerrrainMaterialCacheLODs, lods)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(TerrainSceneData)
	SHADER_STRUCT_FIELD(TerrainHeightMap,                           heightMap)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector3f>,     farLODBaseColor)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector3f>,     farLODProps) // .r = roughness, .g = metallic, .b = occlusion
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<TerrainClipmapTileGPU>,    tiles)
	SHADER_STRUCT_FIELD(Uint32,                                     tilesNum)
	SHADER_STRUCT_FIELD(Real32,                                     tileSizeMeters)
	SHADER_STRUCT_FIELD(Real32,                                     clipmapExtentMeters)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<math::Vector2f>,           meshletVertices)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<Uint32>,                   meshletIndices)
	SHADER_STRUCT_FIELD(Uint32,                                     meshletVerticesNum)
	SHADER_STRUCT_FIELD(Uint32,                                     meshletIndicesNum)
	SHADER_STRUCT_FIELD(Uint32,                                     meshletTranglesNum)
	SHADER_STRUCT_FIELD(TerrrainMaterialCache,                      materialCache)
END_SHADER_STRUCT();

} // spt::rsc
