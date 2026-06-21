#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]
[[shader_params(TerrainBuildTileDrawCommandsConstants, u_constants)]]

#include "Terrain/SceneTerrain.hlsli"
#include "Utils/Culling.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void BuildTerrainTileDrawCommandsCS(CS_INPUT input)
{
	const TerrainInterface terrain = SceneTerrain();

	const uint tileIdx = input.globalID.x;
	if (tileIdx >= terrain.tilesNum)
	{
		return;
	}

	const TerrainClipmapTileGPU tile = terrain.GetTile(tileIdx);

	const float2 tileHeightMinMax = terrain.GetTileHeightMinMax(tile);
	const float3 AABBMin = float3(tile.tileCoordX * terrain.tileSizeMeters, tile.tileCoordY * terrain.tileSizeMeters, tileHeightMinMax.x);
	const float3 AABBMax = float3(AABBMin.x + terrain.tileSizeMeters, AABBMin.y + terrain.tileSizeMeters, tileHeightMinMax.y);

	const bool isTileVisible = IsAABBInFrustum(u_cullingData.cullingPlanes, AABBMin, AABBMax);

	if (isTileVisible)
	{
		const uint outputIdx = u_constants.rwDrawCommandsCount.AtomicAdd(0u, 1u);

		const uint tileLOD = clamp(terrain.GetTileLOD(tileIdx) + u_constants.lodBias, 0, TERRAIN_TILE_MAX_LOD);;
		const uint meshletsRes = tileLOD != IDX_NONE_8 ? (TERRAIN_MESHLETS_PER_TILE << (TERRAIN_TILE_MAX_LOD - tileLOD)) : 0u;

		TerrainDrawMeshTaskCommand drawCommand;
		drawCommand.dispatchGroupsX = meshletsRes;
		drawCommand.dispatchGroupsY = meshletsRes;
		drawCommand.dispatchGroupsZ = 1u;
		drawCommand.drawCommand     = PackTileDrawCommand(tileIdx, tileLOD);

		u_constants.rwDrawCommands.Store(outputIdx, drawCommand);
	}
}
