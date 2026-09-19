#include "SculptorShader.hlsli"


[[shader_params(BuildLightZClustersConstants, CONSTS)]]


struct CS_INPUT
{
    uint3 globalID  : SV_DispatchThreadID;
    uint3 groupID   : SV_GroupID;
    uint3 localID   : SV_GroupThreadID;
};


[numthreads(32, 1, 1)]
void BuildLightsZClustersCS(CS_INPUT input)
{
	const uint clusterIdx = input.localID.x;

	if (clusterIdx < CONSTS->lightsData->zClustersNum)
	{
		const float clusterRangeMin = clusterIdx * CONSTS->lightsData->zClusterLength;
		const float clusterRangeMax = (clusterIdx + 1) * CONSTS->lightsData->zClusterLength;

		uint lightMinIdx = 0xffffffff;

		for (uint lightIdx = 0; lightIdx < CONSTS->lightsData->localLightsNum; lightIdx += 1)
		{
			const float2 lightRange = CONSTS->localLightsZRanges[lightIdx];
			const bool isInRange = lightRange.x < clusterRangeMax && lightRange.y > clusterRangeMin;
			if (isInRange)
			{
				lightMinIdx = lightIdx;
				break;
			}
		}

		uint lightMaxIdx = 0;

		if (lightMinIdx != 0xffffffff)
		{
			for (uint idx = 0; idx < CONSTS->lightsData->localLightsNum; idx += 1)
			{
				const uint lightIdx = CONSTS->lightsData->localLightsNum - idx - 1;
				const float2 lightRange = CONSTS->localLightsZRanges[lightIdx];
				const bool isInRange = lightRange.x < clusterRangeMax && lightRange.y > clusterRangeMin;
				if (isInRange)
				{
					lightMaxIdx = lightIdx;
					break;
				}
			}
		}
		else
		{
			lightMinIdx = 0;
			lightMaxIdx = 0;
		}

		CONSTS->rwClusterRanges.Store(clusterIdx, uint2(lightMinIdx, lightMaxIdx));
	}
}
