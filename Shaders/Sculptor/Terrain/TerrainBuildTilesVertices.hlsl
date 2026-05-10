#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[shader_params(TerrainBuildTilesVerticesConstants, u_constants)]]

#include "Terrain/SceneTerrain.hlsli"


static const uint TERRAIN_MESHLET_QUADS_PER_EDGE    = 7u;
static const uint TERRAIN_MESHLET_VERTICES_PER_EDGE = TERRAIN_MESHLET_QUADS_PER_EDGE + 1u;
static const uint TERRAIN_MESHLET_VERTICES_NUM      = TERRAIN_MESHLET_VERTICES_PER_EDGE * TERRAIN_MESHLET_VERTICES_PER_EDGE;


struct CS_INPUT
{
	uint3 groupID : SV_GroupID;
	uint localID  : SV_GroupIndex;
};


[numthreads(TERRAIN_MESHLET_VERTICES_NUM, 1, 1)]
void BuildTerrainTilesVerticesCS(CS_INPUT input)
{
	const TerrainInterface terrain = SceneTerrain();

	const uint tileIdx = input.groupID.x;
	if (tileIdx >= terrain.tilesNum || input.localID >= terrain.meshletVerticesNum)
	{
		return;
	}

	const TerrainClipmapTileGPU tile = terrain.GetTile(tileIdx);

	const float2 meshletVertex = terrain.meshletVertices.Load(input.localID).xy;
	const float2 tileOffset    = float2(tile.tileCoordX, tile.tileCoordY);
	const float2 locationXY    = (tileOffset + meshletVertex) * terrain.tileSizeMeters;
	const float  height        = terrain.GetHeight(locationXY);

	const uint vertexIdx = tileIdx * terrain.meshletVerticesNum + input.localID;
	u_constants.rwRuntimeVertices.Store(vertexIdx, float3(locationXY, height));
}
