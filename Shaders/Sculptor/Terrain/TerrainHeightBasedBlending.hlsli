#ifndef TERRAIN_HEIGHT_BASED_BLENDING_HLSLI
#define TERRAIN_HEIGHT_BASED_BLENDING_HLSLI

[[shader_struct(MaterialDisplacementData)]]


#ifdef DS_RenderSceneDS

void ApplyHeightBasedWeighting(inout TerrainMaterialsFactors factors, in float2 worldLocation)
{
	TerrainInterface terrain = SceneTerrain();

	float depth = 0.2f;

	float matHeights[4];

	[unroll]
	for (uint idx = 0u; idx < 4u; ++idx)
	{
		matHeights[idx] = 0.f;

		if (factors.materialIDs[idx] != IDX_NONE_8)
		{
			const TerrainMaterialEntry materialEntry = terrain.materialData.matEntries.Load(factors.materialIDs[idx]);
			const MaterialDisplacementData materialDisplacementData = LoadMaterialFeature<MaterialDisplacementData>(materialEntry.dataHandle, materialEntry.displacementMatFeatureID);

			if (materialDisplacementData.displacementTexture.IsValid())
			{
				const float2 uv = worldLocation * materialEntry.uvScale;

				TerrainDetilingSampler sampler = TerrainDetilingSampler::Initialize(uv);
				matHeights[idx] = sampler.Sample(materialDisplacementData.displacementTexture, BindlessSamplers::LinearRepeat(), uv);
			}
		}
	}

	float maxHeight = -9999.f;
	[unroll]
	for (uint idx = 0u; idx < 4u; ++idx)
	{
		if (matHeights[idx] > maxHeight)
		{
			maxHeight = matHeights[idx];
		}
	}

	[unroll]
	for (uint idx = 0u; idx < 4u; ++idx)
	{
		if (factors.materialIDs[idx] != IDX_NONE_8)
		{
			factors.materialWeights[idx] = max(0.f, factors.materialWeights[idx] + (matHeights[idx] - maxHeight) * 6.f);
		}
	}

	float sumWeights = 0.f;
	[unroll]
	for (uint idx = 0u; idx < 4u; ++idx)
	{
		if (factors.materialIDs[idx] != IDX_NONE_8)
		{
			sumWeights += factors.materialWeights[idx];
		}
	}

	const float rcpSumWeights = 1.f / sumWeights;

	[unroll]
	for (uint idx = 0u; idx < 4u; ++idx)
	{
		factors.materialWeights[idx] *= rcpSumWeights;
	}
}

#endif // DS_RenderSceneDS

#endif // TERRAIN_HEIGHT_BASED_BLENDING_HLSLI
