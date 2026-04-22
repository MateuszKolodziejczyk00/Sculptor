#pragma once

#include "RenderSceneMacros.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Bindless/BindlessTypes.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(TerrainClipmapTileGPU)
	SHADER_STRUCT_FIELD(Int32,  tileCoordX)
	SHADER_STRUCT_FIELD(Int32,  tileCoordY)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(TerrainHeightMap)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,                  texture)
	SHADER_STRUCT_FIELD(math::Vector2u,                             res)
	SHADER_STRUCT_FIELD(math::Vector2f,                             invRes)
	SHADER_STRUCT_FIELD(math::Vector2f,                             spanMeters)
	SHADER_STRUCT_FIELD(math::Vector2f,                             invSpanMeters)
	SHADER_STRUCT_FIELD(math::Vector2f,                             metersPerTexel)
	SHADER_STRUCT_FIELD(Real32,                                     minHeight)
	SHADER_STRUCT_FIELD(Real32,                                     maxHeight)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(TerrainSceneData)
	SHADER_STRUCT_FIELD(TerrainHeightMap,                           heightMap)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<TerrainClipmapTileGPU>, tiles)
	SHADER_STRUCT_FIELD(Uint32,                                     tilesNum)
	SHADER_STRUCT_FIELD(Uint32,                                     lodsNum)
	SHADER_STRUCT_FIELD(Real32,                                     tileSizeMeters)
	SHADER_STRUCT_FIELD(Real32,                                     clipmapExtentMeters)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<math::Vector2f>,        meshletVertices)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<Uint32>,                meshletIndices)
	SHADER_STRUCT_FIELD(Uint32,                                     meshletVerticesNum)
	SHADER_STRUCT_FIELD(Uint32,                                     meshletIndicesNum)
	SHADER_STRUCT_FIELD(Uint32,                                     meshletTranglesNum)
END_SHADER_STRUCT();

} // spt::rsc
