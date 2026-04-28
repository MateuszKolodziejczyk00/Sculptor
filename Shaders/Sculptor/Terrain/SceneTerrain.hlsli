#ifndef TERRAIN_HLSLI
#define TERRAIN_HLSLI

[[shader_struct(TerrainSceneData)]]


struct TerrainInterface : TerrainSceneData
{
	TerrainClipmapTileGPU GetTile(uint tileIdx)
	{
		return tiles.Load(tileIdx);
	}

	float2 GetHeightMapUV(float2 locationXY)
	{
		return locationXY * heightMap.invSpanMeters + 0.5f;
	}

	float2 GetHeightSampleOffset(float2 locationXY)
	{
		return heightMap.metersPerTexel;
	}

	float GetHeight(float2 locationXY)
	{
		const float heightMapValue = heightMap.texture.SampleLevel(BindlessSamplers::LinearClampEdge(), GetHeightMapUV(locationXY), 0);
		return lerp(heightMap.minHeight, heightMap.maxHeight, heightMapValue);
	}

	float2 GetHeightDerivatives(float2 locationXY)
	{
		const float2 sampleOffset = GetHeightSampleOffset(locationXY);

#if 1
		const float heightPosXPosY = GetHeight(locationXY + float2(sampleOffset.x, sampleOffset.y));
		const float heightPosX     = GetHeight(locationXY + float2(sampleOffset.x, 0.f));
		const float heightPosXNegY = GetHeight(locationXY + float2(sampleOffset.x, -sampleOffset.y));
		const float heightPosY     = GetHeight(locationXY + float2(0.f, sampleOffset.y));
		const float heightNegY     = GetHeight(locationXY + float2(0.f, -sampleOffset.y));
		const float heightNegXPosY = GetHeight(locationXY + float2(-sampleOffset.x, sampleOffset.y));
		const float heightNegX     = GetHeight(locationXY + float2(-sampleOffset.x, 0.f));
		const float heightNegXNegY = GetHeight(locationXY + float2(-sampleOffset.x, -sampleOffset.y));

		const float dHeightDX = ((heightPosXNegY + 2.f * heightPosX + heightPosXPosY) - (heightNegXNegY + 2.f * heightNegX + heightNegXPosY)) / (8.f * sampleOffset.x);
		const float dHeightDY = ((heightNegXPosY + 2.f * heightPosY + heightPosXPosY) - (heightNegXNegY + 2.f * heightNegY + heightPosXNegY)) / (8.f * sampleOffset.y);

		return float2(dHeightDX, dHeightDY);
#else
		const float heightXP = GetHeight(locationXY + float2(sampleOffset.x, 0.f));
		const float heightXM = GetHeight(locationXY - float2(sampleOffset.x, 0.f));
		const float heightYP = GetHeight(locationXY + float2(0.f, sampleOffset.y));
		const float heightYM = GetHeight(locationXY - float2(0.f, sampleOffset.y));

		return float2((heightXP - heightXM) / (2.f * sampleOffset.x),
					  (heightYP - heightYM) / (2.f * sampleOffset.y));
#endif
	}

	float3 GetNormal(float2 locationXY)
	{
		const float2 heightDerivatives = GetHeightDerivatives(locationXY);
		return normalize(float3(-heightDerivatives.x, -heightDerivatives.y, 1.f));
	}

	float3 GetTangent(float2 locationXY)
	{
		const float dHeightDX = GetHeightDerivatives(locationXY).x;
		return normalize(float3(1.f, 0.f, dHeightDX));
	}

	float3 GetBitangent(float2 locationXY)
	{
		const float dHeightDY = GetHeightDerivatives(locationXY).y;
		return normalize(float3(0.f, 1.f, dHeightDY));
	}
};


#if defined(DS_RenderSceneDS)
TerrainInterface SceneTerrain()
{
	return TerrainInterface(u_renderSceneConstants.terrain);
}
#endif // DS_RenderSceneDS

#endif // TERRAIN_HLSLI
