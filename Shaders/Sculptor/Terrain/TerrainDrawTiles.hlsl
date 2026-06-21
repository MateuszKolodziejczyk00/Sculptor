#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]
[[descriptor_set(TerrainVisibilityDS)]]

#include "Terrain/SceneTerrain.hlsli"
#include "GeometryRendering/GeometryCommon.hlsli"


#define TERRAIN_VISIBILITY_MS_GROUP_SIZE 64u


static const uint TERRAIN_MESHLET_QUADS_PER_EDGE    = 8u;
static const uint TERRAIN_MESHLET_VERTICES_PER_EDGE = TERRAIN_MESHLET_QUADS_PER_EDGE + 1u;
static const uint TERRAIN_MESHLET_VERTICES_NUM      = TERRAIN_MESHLET_VERTICES_PER_EDGE * TERRAIN_MESHLET_VERTICES_PER_EDGE;
static const uint TERRAIN_MESHLET_TRIANGLES_NUM     = TERRAIN_MESHLET_QUADS_PER_EDGE * TERRAIN_MESHLET_QUADS_PER_EDGE * 2u;


struct MeshVertex
{
	float4 clipSpace : SV_Position;
};


struct MeshShaderInput
{
	uint3 meshletID : SV_GroupID;
	uint localID    : SV_GroupIndex;
	[[vk::builtin("DrawIndex")]] uint drawCommandIndex : DRAW_INDEX;
};


// Assumes that TERRAIN_MESHLET_VERTICES_PER_EDGE is a power of 2
uint ApplyLODChangeMask(in uint vertexIdx, in uint changeMask, in uint2 meshletID, in uint2 meshletsRes)
{
	const uint2 vertexPos = uint2(vertexIdx % TERRAIN_MESHLET_VERTICES_PER_EDGE, vertexIdx / TERRAIN_MESHLET_VERTICES_PER_EDGE);

	if (meshletID.y == 0u)
	{
		if ((changeMask & TERRAIN_TILE_LOD_CHANGE_NORTH) && vertexPos.y == 0u)
		{
			const uint mask = TERRAIN_TILE_LOD_CHANGE_NORTH_4 ? 3u : 1u;
			return vertexIdx & ~mask;
		}
	}

	if (meshletID.y == meshletsRes.y - 1u)
	{
		if ((changeMask & TERRAIN_TILE_LOD_CHANGE_SOUTH) && vertexPos.y == TERRAIN_MESHLET_VERTICES_PER_EDGE - 1u)
		{
			const uint mask = TERRAIN_TILE_LOD_CHANGE_SOUTH_4 ? 3u : 1u;
			return vertexIdx & ~mask;
		}
	}

	if (meshletID.x == 0u)
	{
		if ((changeMask & TERRAIN_TILE_LOD_CHANGE_WEST) && vertexPos.x == 0u)
		{
			const uint mask = TERRAIN_TILE_LOD_CHANGE_WEST_4 ? 3u : 1u;
			return ((vertexIdx / TERRAIN_MESHLET_VERTICES_PER_EDGE) & ~mask) * TERRAIN_MESHLET_VERTICES_PER_EDGE;
		}
	}

	if (meshletID.x == meshletsRes.x - 1u)
	{
		if ((changeMask & TERRAIN_TILE_LOD_CHANGE_EAST) && vertexPos.x == TERRAIN_MESHLET_VERTICES_PER_EDGE - 1u)
		{
			const uint mask = TERRAIN_TILE_LOD_CHANGE_EAST_4 ? 3u : 1u;
			return ((vertexIdx / TERRAIN_MESHLET_VERTICES_PER_EDGE) & ~mask) * TERRAIN_MESHLET_VERTICES_PER_EDGE + TERRAIN_MESHLET_VERTICES_PER_EDGE - 1u;
		}
	}

	return vertexIdx;
}


[outputtopology("triangle")]
[numthreads(TERRAIN_VISIBILITY_MS_GROUP_SIZE, 1, 1)]
void Terrain_MS(in MeshShaderInput input,
						  out vertices MeshVertex outVertices[TERRAIN_MESHLET_VERTICES_NUM],
						  out indices uint3 outTriangles[TERRAIN_MESHLET_TRIANGLES_NUM])
{
	const uint verticesNum  = TERRAIN_MESHLET_VERTICES_NUM;
	const uint trianglesNum = TERRAIN_MESHLET_TRIANGLES_NUM;
	SetMeshOutputCounts(verticesNum, trianglesNum);

	const TerrainInterface terrain = SceneTerrain();

	const TerrainDrawMeshTaskCommand drawCommand = u_constants.drawCommands.Load(input.drawCommandIndex);

	uint tileIdx, tileLOD;
	UnpackTileDrawCommand(drawCommand.drawCommand     , OUT tileIdx, OUT tileLOD);

	const TerrainClipmapTileGPU tile = terrain.GetTile(tileIdx);
	uint tileLODChangeMask;
	terrain.GetTileLODAndLODChangeMask(tileIdx, OUT tileLODChangeMask);

	const float tileSizeMeters = u_renderSceneConstants.terrain.tileSizeMeters;
	const float2 tileOffset = float2(tile.tileCoordX, tile.tileCoordY);

	const uint meshletsRes = TERRAIN_MESHLETS_PER_TILE << (TERRAIN_TILE_MAX_LOD - tileLOD);
	const float meshletSize = 1.f / meshletsRes;
	const float2 meshletOffset = tileOffset + float2(input.meshletID.x, input.meshletID.y) * meshletSize;

	for (uint it = input.localID; it < verticesNum; it += TERRAIN_VISIBILITY_MS_GROUP_SIZE)
	{
		// Don't apply LOD change mask for vertices if tile is at max LOD
		// This could happen due to LOD bias
		const uint vertexIdx = tileLOD != TERRAIN_TILE_MAX_LOD ? ApplyLODChangeMask(it, tileLODChangeMask, input.meshletID.xy, meshletsRes) : it;
		const float2 meshletVertex = terrain.meshletVertices.Load(vertexIdx).xy;
		const float2 locationXY    = (meshletOffset + meshletVertex * meshletSize) * tileSizeMeters;
		const float3 normal        = terrain.GetNormal(locationXY);
		const float height         = terrain.GetHeightSmooth(locationXY);

		const float3 worldPos = float3(locationXY, height) + terrain.GetDisplacement(locationXY) * normal;

		outVertices[it].clipSpace = mul(u_sceneView.viewProjectionMatrix, float4(worldPos, 1.f));
	}

	for (uint triangleIdx = input.localID; triangleIdx < trianglesNum; triangleIdx += TERRAIN_VISIBILITY_MS_GROUP_SIZE)
	{
		const uint indicesOffset = triangleIdx * 3u;
		outTriangles[triangleIdx] = uint3(terrain.meshletIndices.Load(indicesOffset),
										  terrain.meshletIndices.Load(indicesOffset + 1u),
										  terrain.meshletIndices.Load(indicesOffset + 2u));
	}
}


struct FS_VIS_OUTPUT
{
	uint visibility : SV_TARGET0;
};


FS_VIS_OUTPUT TerrainVisibility_FS()
{
	FS_VIS_OUTPUT output;
	output.visibility = PackTerrainVisibilityInfo();
	return output;
}


void TerrainDepth_FS()
{
}
