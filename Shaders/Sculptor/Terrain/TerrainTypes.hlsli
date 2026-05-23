#ifndef TERRAIN_TYPES_HLSLI
#define TERRAIN_TYPES_HLSLI

struct TerrainMaterialsFactors
{
	uint4  materialIDs;
	float4 materialWeights;
};


[[shader_struct(TerrainMaterialsMap)]]


TerrainMaterialsFactors SampleTerrainMaterialsMap(in TerrainMaterialsMap materialsMap, in float2 worldLocation)
{
	const float2 uv = saturate((worldLocation - materialsMap.minBounds) * materialsMap.rcpBoundsSize);

	const int2 textureResolution = max(int2(materialsMap.resolution), int2(1, 1));
	const float2 bilinearCoords  = uv * materialsMap.resolution - 0.5f;
	const int2 texel00           = clamp(int2(floor(bilinearCoords)), int2(0, 0), textureResolution - 1);
	const int2 texel11           = min(texel00 + int2(1, 1), textureResolution - 1);
	const int2 texel10           = int2(texel11.x, texel00.y);
	const int2 texel01           = int2(texel00.x, texel11.y);

	const uint4 materialIDs = uint4(materialsMap.materialIDs.Load(int3(texel00, 0)),
									materialsMap.materialIDs.Load(int3(texel10, 0)),
									materialsMap.materialIDs.Load(int3(texel01, 0)),
									materialsMap.materialIDs.Load(int3(texel11, 0)));

	const float2 bilinearFrac    = frac(bilinearCoords);
	const float2 invBilinearFrac = 1.f - bilinearFrac;
	float4 materialWeights       = float4(invBilinearFrac.x * invBilinearFrac.y,
										  bilinearFrac.x * invBilinearFrac.y,
										  invBilinearFrac.x * bilinearFrac.y,
										  bilinearFrac.x * bilinearFrac.y);

	[unroll]
	for (uint materialIdx = 0u; materialIdx < 4u; ++materialIdx)
	{
		[unroll]
		for (uint prevMaterialIdx = 0u; prevMaterialIdx < materialIdx; ++prevMaterialIdx)
		{
			if (materialIDs[materialIdx] == materialIDs[prevMaterialIdx])
			{
				materialWeights[prevMaterialIdx] += materialWeights[materialIdx];
				materialWeights[materialIdx] = 0.f;
				break;
			}
		}
	}

	TerrainMaterialsFactors factors;
	factors.materialIDs     = materialIDs;
	factors.materialWeights = materialWeights;

	return factors;
}

#endif // TERRAIN_TYPES_HLSLI
