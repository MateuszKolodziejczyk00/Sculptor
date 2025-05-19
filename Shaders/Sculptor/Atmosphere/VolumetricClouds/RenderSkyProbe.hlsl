#include "SculptorShader.hlsli"

[[descriptor_set(RenderSkyProbeDS, 0)]]

#include "Atmosphere/Atmosphere.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};

#define GROUP_SIZE 256
#define WAVES_NUM (GROUP_SIZE / 32)

groupshared float3 gs_luminanceSum[WAVES_NUM];

[numthreads(GROUP_SIZE, 1, 1)]
void RenderSkyProbeCS(CS_INPUT input)
{
    const uint threadIdx = input.globalID.x;

	const float3 direction = FibbonaciSphereDistribution(threadIdx, GROUP_SIZE);

	const float3 viewLocation = GetLocationInAtmosphere(u_atmosphereParams, 0.f);

	const float3 skyLuminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, viewLocation, direction);

    const float3 waveSumLuminance = WaveActiveSum(skyLuminance / GROUP_SIZE);

    if(WaveIsFirstLane())
    {
        gs_luminanceSum[threadIdx / WaveGetLaneCount()] = waveSumLuminance;
    }

    GroupMemoryBarrierWithGroupSync();

    float3 fetchedLuminance = 0.f;
    if(threadIdx < WAVES_NUM)
    {
        fetchedLuminance = gs_luminanceSum[threadIdx];
    }

    const float3 finalLuminance = WaveActiveSum(fetchedLuminance);

    if(threadIdx == 0u)
    {
        u_rwProbe[uint2(0, 0)] = finalLuminance;
    }
}