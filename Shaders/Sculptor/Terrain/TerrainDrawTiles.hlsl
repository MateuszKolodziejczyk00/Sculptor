#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]
[[descriptor_set(TerrainVisibilityDS)]]

#include "Terrain/SceneTerrain.hlsli"
#include "GeometryRendering/GeometryCommon.hlsli"


#define TERRAIN_VISIBILITY_MS_GROUP_SIZE 64u


static const uint TERRAIN_MESHLET_QUADS_PER_EDGE    = 7u;
static const uint TERRAIN_MESHLET_VERTICES_PER_EDGE = TERRAIN_MESHLET_QUADS_PER_EDGE + 1u;
static const uint TERRAIN_MESHLET_VERTICES_NUM      = TERRAIN_MESHLET_VERTICES_PER_EDGE * TERRAIN_MESHLET_VERTICES_PER_EDGE;
static const uint TERRAIN_MESHLET_TRIANGLES_NUM     = TERRAIN_MESHLET_QUADS_PER_EDGE * TERRAIN_MESHLET_QUADS_PER_EDGE * 2u;


struct MeshVertex
{
	float4 clipSpace : SV_Position;
};


struct MeshShaderInput
{
	uint localID : SV_GroupIndex;
	[[vk::builtin("DrawIndex")]] uint drawCommandIndex : DRAW_INDEX;
};


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

	const TerrainClipmapTileGPU tile = terrain.GetTile(drawCommand.visibleTileIdx);

	const float tileSizeMeters = u_renderSceneConstants.terrain.tileSizeMeters;
	const float2 tileOffset = float2(tile.tileCoordX, tile.tileCoordY);

	for (uint vertexIdx = input.localID; vertexIdx < verticesNum; vertexIdx += TERRAIN_VISIBILITY_MS_GROUP_SIZE)
	{
		const float2 meshletVertex = terrain.meshletVertices.Load(vertexIdx).xy;
		const float2 locationXY    = (tileOffset + meshletVertex - 0.5f.xx) * tileSizeMeters;
		const float height         = terrain.GetHeight(locationXY);

		outVertices[vertexIdx].clipSpace = mul(u_sceneView.viewProjectionMatrix, float4(locationXY, height, 1.f));
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
