#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[shader_params(TerrainBuildTileDrawCommandsConstants, u_constants)]]

#include "Terrain/SceneTerrain.hlsli"


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
	const bool isTileVisible = true;

	if (isTileVisible)
	{
		const uint outputIdx = u_constants.rwDrawCommandsCount.AtomicAdd(0u, 1u);

		TerrainDrawMeshTaskCommand drawCommand;
		drawCommand.dispatchGroupsX = 1u;
		drawCommand.dispatchGroupsY = 1u;
		drawCommand.dispatchGroupsZ = 1u;
		drawCommand.visibleTileIdx  = tileIdx;

		u_constants.rwDrawCommands.Store(outputIdx, drawCommand);
	}
}
