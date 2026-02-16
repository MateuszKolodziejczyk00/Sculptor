#ifndef WSC_HLSLI
#define WSC_HLSLI

[[shader_struct(SceneGeometryData)]]


#define CASCADES_NUM 2


struct WSCInterface : WSCData
{
	float SampleShadows(in float3 worldPos)
	{
		float3 shadowMapNDC = 0.0f;
		SRVTexture2D<float> shadowMap;
		bool foundCascade = false;

		for (int cascadeIndex = 0; cascadeIndex < CASCADES_NUM; ++cascadeIndex)
		{
			const float4 posCS = mul(dirLightsData.cascades[cascadeIndex].viewProj, float4(worldPos, 1.0f));

			if (all(abs(posCS.xy) <= posCS.w))
			{
				shadowMapNDC = posCS.xyz / posCS.w;
				shadowMap = dirLightsData.cascades[cascadeIndex].shadowMap;
				foundCascade = true;
				break;
			}
		}

		if (foundCascade)
		{
			const float2 shadowMapUV = shadowMapNDC.xy * 0.5f + 0.5f;
			const float z = shadowMap.SampleLevel(BindlessSamplers::NearestClampEdge(), shadowMapUV, 0.0f);
			return (z < shadowMapNDC.z) ? 1.0f : 0.0f;
		}
		else
		{
			return 1.0f;
		}
	}

	float SampleShadows(in float3 worldPos, in float3 normal)
	{
		return SampleShadows(worldPos + normal * 0.04f);
	}
};


#ifdef DS_RenderSceneDS

WSCInterface WSC()
{
	return WSCInterface(u_renderSceneConstants.wsc);
}

#endif // DS_RenderSceneDS

#endif // WSC_HLSLI
