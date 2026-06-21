#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]
[[shader_params(GenerateGrassBladesDefsConstants, u_constants)]]

#include "Terrain/SceneTerrain.hlsli"
#include "Utils/Random.hlsli"
#include "Terrain/Grass/GrassGeometry.hlsli"
#include "Utils/Culling.hlsli"
#include "Utils/Wave.hlsli"
#include "Terrain/TerrainMaterial.hlsli"
#include "Terrain/TerrainHeightBasedBlending.hlsli"


#define GRASS_TILE_SIZE 4.f


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};



int2 FindClump(in float2 bladeLocation)
{
	const float clumpDist = 2.f;

	const int2 nearestClumpCoord = int2(round(bladeLocation / clumpDist));

	float minDistance2 = 1e10f;
	int2 closestClumpCoord;
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			const int2 clumpCoord = nearestClumpCoord + int2(x, y);
			const float2 clumpCenter = float2(clumpCoord) * clumpDist;

			const uint2 clumpCoord8 = uint2(clumpCoord + 256 * 256) & 255u;
			RngState rng = RngState::Create(clumpCoord8, 0u);

			const float2 offset = (float2(rng.Next(), rng.Next()) - 0.5f) * clumpDist;
			const float2 clumpLocation = clumpCenter + offset;

			const float2 bladeToClump = clumpLocation - bladeLocation;
			const float distance2 = dot(bladeToClump, bladeToClump);

			if (distance2 < minDistance2)
			{
				minDistance2         = distance2;
				closestClumpCoord    = clumpCoord;
			}
		}
	}

	return closestClumpCoord;
}


uint GetGrassTypeAtLocation(inout RngState rng, in float2 location)
{
	const TerrainInterface terrain = SceneTerrain();

	TerrainMaterialsFactors materialFactors = terrain.GetMaterialsFactors(location);
	ApplyHeightBasedWeighting(materialFactors, location);

	float random = rng.Next();
	uint materialID = 0u;
	for (uint i = 0u; i < 4u; ++i)
	{
		random -= materialFactors.materialWeights[i];
		if (random <= 0.f)
		{
			materialID = materialFactors.materialIDs[i];
			break;
		}
	}

	return terrain.materialData.matEntries.Load(materialID).grassType;
}


void GenerateImpl(int2 coord, uint lod)
{
	RngState rng = RngState::Create(coord, 0u);

	const float2 spacing = GRASS_TILE_SIZE / 32.f;
	const float2 pos2d = (float2(coord) + (float2(rng.Next(), rng.Next()) - 0.5f)) * spacing;

	const uint grassType = GetGrassTypeAtLocation(rng, pos2d);

	if (grassType == IDX_NONE_8)
	{
		return;
	}

	const int2 clumpCoord = FindClump(pos2d);
	const uint2 clumpCoord8 = uint2(clumpCoord + 256 * 256) & 255u;

	GrassBladeDef bladeDef;
	bladeDef.location    = pos2d;
	bladeDef.flags       = uint16_t((lod == 0u) ? GRASS_BLADE_FLAGS_NONE : GRASS_BLADE_FLAGS_LOD_1);
	bladeDef.clumpCoords = uint16_t(clumpCoord8.x | (clumpCoord8.y << 8u));

	const uint2 activeBallot = WaveActiveBallot(true).xy;
	const uint outputBladesNum = countbits(activeBallot.x) + countbits(activeBallot.y);

	uint offset = 0u;
	if (WaveIsFirstLane())
	{
		offset = u_constants.rwBladesNumLODs[lod].AtomicAdd(0u, outputBladesNum);
	}
	offset = WaveReadLaneFirst(offset) + GetCompactedIndex(activeBallot, WaveGetLaneIndex());

	u_constants.rwBladeDefsLODs[lod].Store(offset, bladeDef);
}


bool IsLOD0(int2 tileCoord)
{
	const float2 tileCenter = (float2(tileCoord) + 0.5f) * GRASS_TILE_SIZE;

	const float2 viewLocation = u_sceneView.viewLocation.xy;

	const float distance = length(tileCenter - viewLocation);

	return distance < 64.f;
}


[numthreads(16, 16, 1)]
void GenerateGrassBladesDefsCS(CS_INPUT input)
{
	const int2 tileCoord = u_constants.originTile + input.groupID.xy;

	const float inflate = 2.f;

	const float2 tileMin = float2(tileCoord) * GRASS_TILE_SIZE - inflate;
	const float2 tileMax = tileMin + GRASS_TILE_SIZE + inflate;
	const float2 tileCenter = (tileMin + tileMax) * 0.5f;
	const float2 tileMinMaxHeight = SceneTerrain().GetTileHeightMinMaxAtLocation(tileCenter);
	const float averageZ = (tileMinMaxHeight.x + tileMinMaxHeight.y) * 0.5f;
	const float radius = length(float3(GRASS_TILE_SIZE, GRASS_TILE_SIZE, (tileMinMaxHeight.y - tileMinMaxHeight.x) * 0.5f) + inflate);
	const bool isTileVisible = IsSphereInFrustum(u_cullingData.cullingPlanes, float3(tileCenter, averageZ), radius);

	if (!isTileVisible)
	{
		return;
	}

	const bool isLod0 = WaveReadLaneFirst(IsLOD0(tileCoord));
	const uint lod = isLod0 ? 0u : 1u;

	int2 coord = tileCoord * 32 + input.localID.xy * 2;

	GenerateImpl(coord, lod);

	if (isLod0)
	{
		GenerateImpl(coord + uint2(1, 0), lod);
		GenerateImpl(coord + uint2(0, 1), lod);
		GenerateImpl(coord + uint2(1, 1), lod);
	}
}
