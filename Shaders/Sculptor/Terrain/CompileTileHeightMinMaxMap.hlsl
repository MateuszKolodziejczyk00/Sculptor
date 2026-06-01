#include "SculptorShader.hlsli"

[[shader_params(CompileTileHeightMinMaxMapConstants, u_constants)]]


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void CompileTileHeightMinMaxMapCS(CS_INPUT input)
{
	const int2 tileCoords      = int2(input.globalID.xy);
	const int2 tileResolution  = int2(u_constants.rwTileHeightMinMaxMap.GetResolution());
	const int2 heightMapMaxTexel = int2(u_constants.heightMapResolution) - 1;

	if (any(tileCoords >= tileResolution))
	{
		return;
	}

	const float2 terrainSize = u_constants.maxBounds - u_constants.minBounds;
	const float2 tileMin     = u_constants.minBounds + float2(tileCoords) * u_constants.tileSizeMeters;
	const float2 tileMax     = min(tileMin + u_constants.tileSizeMeters, u_constants.maxBounds);

	const float2 uvMin = saturate((tileMin - u_constants.minBounds) / terrainSize);
	const float2 uvMax = saturate((tileMax - u_constants.minBounds) / terrainSize);

	const float2 heightMapResolution = float2(u_constants.heightMapResolution);
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
			const float height = u_constants.heightMap.Load(int2(x, y));
			minHeight = min(minHeight, height);
			maxHeight = max(maxHeight, height);
		}
	}

	u_constants.rwTileHeightMinMaxMap.Store(tileCoords, float2(minHeight, maxHeight));
}
