#include "SculptorShader.hlsli"


[[descriptor_set(BuildLightZClustersDS, 0)]]


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

    if(clusterIdx < u_lightsData.zClustersNum)
    {
        const float clusterRangeMin = clusterIdx * u_lightsData.zClusterLength;
        const float clusterRangeMax = (clusterIdx + 1) * u_lightsData.zClusterLength;

        uint lightMinIdx = 0xffffffff;

        for (uint lightIdx = 0; lightIdx < u_lightsData.localLightsNum; lightIdx += 1)
        {
            const float2 lightRange = u_localLightsZRanges[lightIdx];
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
            for (uint idx = 0; idx < u_lightsData.localLightsNum; idx += 1)
            {
                const uint lightIdx = u_lightsData.localLightsNum - idx - 1;
                const float2 lightRange = u_localLightsZRanges[lightIdx];
                const bool isInRange = lightRange.x < clusterRangeMax && lightRange.y > clusterRangeMin;
                if (isInRange)
                {
                    lightMaxIdx = lightIdx;
                    break;
                }
            }
        }

        u_clustersRanges[clusterIdx] = uint2(lightMinIdx, lightMaxIdx);
    }
}
