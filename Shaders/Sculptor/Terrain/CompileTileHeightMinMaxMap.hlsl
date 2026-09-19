#include "SculptorShader.hlsli"

[[shader_params(CompileTileHeightMinMaxMapConstants, PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void CompileTileHeightMinMaxMapCS(CS_INPUT input)
{
	const int2 tileCoords      = int2(input.globalID.xy);
	const int2 tileResolution  = int2(PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->rwTileHeightMinMaxMap.GetResolution());
	const int2 heightMapMaxTexel = int2(PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->heightMapResolution) - 1;

	if (any(tileCoords >= tileResolution))
	{
		return;
	}

	const float2 terrainSize = PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->maxBounds - PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->minBounds;
	const float2 tileMin     = PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->minBounds + float2(tileCoords) * PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->tileSizeMeters;
	const float2 tileMax     = min(tileMin + PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->tileSizeMeters, PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->maxBounds);

	const float2 uvMin = saturate((tileMin - PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->minBounds) / terrainSize);
	const float2 uvMax = saturate((tileMax - PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->minBounds) / terrainSize);

	const float2 heightMapResolution = float2(PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->heightMapResolution);
	int2 minTexel = int2(floor(uvMin * heightMapResolution - 0.5f));
	int2 maxTexel = int2(floor(uvMax * heightMapResolution - 0.5f)) + 1;

	minTexel = clamp(minTexel, int2(0, 0), heightMapMaxTexel);
	maxTexel = clamp(maxTexel, int2(0, 0), heightMapMaxTexel);

	float minHeight = 1.f;
	float maxHeight = 0.f;

	for (int y = minTexel.y; y <= maxTexel.y; ++y)
	{
		for (int x = minTexel.x; x <= maxTexel.x; ++x)
		{
			const float height = PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->heightMap.Load(int2(x, y));
			minHeight = min(minHeight, height);
			maxHeight = max(maxHeight, height);
		}
	}

	PARAMS_COMPILE_TILE_HEIGHT_MIN_MAX_MAP_CONSTANTS->rwTileHeightMinMaxMap.Store(tileCoords, float2(minHeight, maxHeight));
}
