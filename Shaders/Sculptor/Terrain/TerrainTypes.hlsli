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

	const float2 bilinearCoords  = uv * materialsMap.resolution - 0.5f + SPT_SUB_PIXEL_PRECITION_OFFSET;

	const uint4 materialIDs = materialsMap.materialIDs.Gather<uint4>(BindlessSamplers::NearestClampEdge(), uv);

	const float2 bilinearFrac    = frac(bilinearCoords);
	const float2 invBilinearFrac = 1.f - bilinearFrac;

	float4 materialWeights       = float4(invBilinearFrac.x * bilinearFrac.y,
										  bilinearFrac.x * bilinearFrac.y,
										  bilinearFrac.x * invBilinearFrac.y,
										  invBilinearFrac.x * invBilinearFrac.y);

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
