#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[shader_params(TerrainBuildTilesVerticesConstants, u_constants)]]

#include "Terrain/SceneTerrain.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void BuildTileVertexBufferCS(CS_INPUT input)
{
	const TerrainInterface terrain = SceneTerrain();

	const uint vertexIdx = input.globalID.x;
	if (vertexIdx >= u_constants.verticesPerEdge * u_constants.verticesPerEdge)
	{
		return;
	}

	const TerrainClipmapTileGPU tile = terrain.GetTile(u_constants.tileIdx);

	const uint2 localVertexCoord = uint2(vertexIdx % u_constants.verticesPerEdge, vertexIdx / u_constants.verticesPerEdge);
	const float2 tileOffset      = float2(tile.tileCoordX, tile.tileCoordY) * terrain.tileSizeMeters;
	const float2 locationXY      = tileOffset + localVertexCoord * u_constants.verticesSpacing;
	const float  height          = terrain.GetHeight(locationXY);

	u_constants.rwVertexBuffer.Store(vertexIdx, float3(locationXY, height));
}
