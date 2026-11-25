#include "SculptorShader.hlsli"


[[shader_params(BuildLightZClustersConstants, CONSTS)]]

#include "Lights/LightingUtils.hlsli"


struct CS_INPUT
{
	uint3 groupID   : SV_GroupID;
	uint3 localID   : SV_GroupThreadID;
};


uint2 ReduceClusterLightsRange(uint lightMinIdx, uint lightMaxIdx)
{
	lightMinIdx = WaveActiveMin(lightMinIdx);
	lightMaxIdx = WaveActiveMax(lightMaxIdx);
	return lightMinIdx != 0xffffffff ? uint2(lightMinIdx, lightMaxIdx) : uint2(0u, 0u);
}


[numthreads(32, 1, 1)]
void BuildLightsZClustersCS(CS_INPUT input)
{
	const uint clusterIdx = input.groupID.x;

	const float clusterRangeMin = clusterIdx * CONSTS->lightsData->zClusterLength;
	const float clusterRangeMax = (clusterIdx + 1) * CONSTS->lightsData->zClusterLength;

	uint diLightMinIdx = 0xffffffff;
	uint diLightMaxIdx = 0u;
	uint scatteringLightMinIdx = 0xffffffff;
	uint scatteringLightMaxIdx = 0u;

	for (uint lightIdx = input.localID.x; lightIdx < CONSTS->lightsData->localLightsNum; lightIdx += 32u)
	{
		const float2 lightRange = CONSTS->localLightsZRanges[lightIdx];
		if (lightRange.x < clusterRangeMax && lightRange.y > clusterRangeMin)
		{
			const uint flags = CONSTS->localLights[lightIdx].flags;

			if ((flags & LIGHT_FLAG_CONTRIBUTE_TO_DI) != 0u)
			{
				diLightMinIdx = min(diLightMinIdx, lightIdx);
				diLightMaxIdx = lightIdx;
			}

			if ((flags & LIGHT_FLAG_CONTRIBUTE_TO_SCATTERING) != 0u)
			{
				scatteringLightMinIdx = min(scatteringLightMinIdx, lightIdx);
				scatteringLightMaxIdx = lightIdx;
			}
		}
	}

	const uint2 diRange         = ReduceClusterLightsRange(diLightMinIdx, diLightMaxIdx);
	const uint2 scatteringRange = ReduceClusterLightsRange(scatteringLightMinIdx, scatteringLightMaxIdx);

	if (input.localID.x == 0u)
	{
		CONSTS->rwDIClusterRanges.Store(clusterIdx, diRange);
		CONSTS->rwScatteringClusterRanges.Store(clusterIdx, scatteringRange);
	}
}
