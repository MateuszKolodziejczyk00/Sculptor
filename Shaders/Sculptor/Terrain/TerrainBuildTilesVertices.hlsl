#include "SculptorShader.hlsli"

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(TerrainBuildTilesVerticesConstants, PARAMS_TERRAIN_BUILD_TILES_VERTICES_CONSTANTS)]]

#include "Terrain/SceneTerrain.hlsli"
#include "SceneRendering/GPUScene.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void BuildTileVertexBufferCS(CS_INPUT input)
{
	const TerrainInterface terrain = SceneTerrain();

	const uint vertexIdx = input.globalID.x;
	if (vertexIdx >= PARAMS_TERRAIN_BUILD_TILES_VERTICES_CONSTANTS->verticesPerEdge * PARAMS_TERRAIN_BUILD_TILES_VERTICES_CONSTANTS->verticesPerEdge)
	{
		return;
	}

	const TerrainClipmapTileGPU tile = terrain.GetTile(PARAMS_TERRAIN_BUILD_TILES_VERTICES_CONSTANTS->tileIdx);

	const uint2 localVertexCoord = uint2(vertexIdx % PARAMS_TERRAIN_BUILD_TILES_VERTICES_CONSTANTS->verticesPerEdge, vertexIdx / PARAMS_TERRAIN_BUILD_TILES_VERTICES_CONSTANTS->verticesPerEdge);
	const float2 tileOffset      = float2(tile.tileCoordX, tile.tileCoordY) * terrain.tileSizeMeters;
	const float2 locationXY      = tileOffset + localVertexCoord * PARAMS_TERRAIN_BUILD_TILES_VERTICES_CONSTANTS->verticesSpacing;
	const float  height          = terrain.GetHeight(locationXY);

	PARAMS_TERRAIN_BUILD_TILES_VERTICES_CONSTANTS->rwVertexBuffer.Store(vertexIdx, float3(locationXY, height));
}
