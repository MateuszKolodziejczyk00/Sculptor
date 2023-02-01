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
    const uint clusterIdx = input.groupID.x;

    const float clusterRangeMin = clusterIdx * u_sceneLightsInfo.zClusterLength;
    const float clusterRangeMax = (clusterIdx + 1) * u_sceneLightsInfo.zClusterLength;

    uint lightMinIdx = 0xffffffff;

    for (uint lightIdx = input.localID.x; lightIdx < u_sceneLightsInfo.localLightsNum; lightIdx += 32)
    {
        const float2 lightRange = u_localLightsRanges[lightIdx];
        bool isInRange = lightRange.x < clusterRangeMax && lightRange.y > clusterRangeMin;

        if(isInRange)
        {
            if (WaveIsFirstLane())
            {
                lightMinIdx = lightIdx;
                break;
            }
        }
    }

    uint lightMaxIdx = 0;

    if(lightMinIdx != 0xffffffff)
    {
        for (uint idx = input.localID.x; idx < u_sceneLightsInfo.localLightsNum; idx += 32)
        {
            const uint lightIdx = u_sceneLightsInfo.localLightsNum - lightIdx - 1;
            const float2 lightRange = u_localLightsRanges[lightIdx];
            bool isInRange = lightRange.x < clusterRangeMax && lightRange.y > clusterRangeMin;

            if (isInRange)
            {
                if (WaveIsFirstLane())
                {
                    lightMaxIdx = lightIdx;
                    break;
                }
            }
        }
    }

    u_clustersRanges[clusterIdx] = uint2(lightMinIdx, lightMaxIdx);
}
