#include "SculptorShader.hlsli"

[[descriptor_set(RenderSkyProbeDS, 0)]]

#include "Atmosphere/Atmosphere.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};

#define GROUP_SIZE 256
#define WAVES_NUM (GROUP_SIZE / 32)

groupshared float4 gs_luminanceSum[WAVES_NUM];

[numthreads(GROUP_SIZE, 1, 1)]
void RenderSkyProbeCS(CS_INPUT input)
{
    const uint threadIdx = input.globalID.x;

	float3 direction = FibbonaciSphereDistribution(threadIdx, GROUP_SIZE);
	direction.z = abs(direction.z);

	const float3 viewLocation = GetLocationInAtmosphere(u_atmosphereParams, 0.f);

	const float3 skyLuminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, viewLocation, direction);
	const float weight = direction.z;

    const float4 waveSumLuminance = WaveActiveSum(float4(skyLuminance * weight / GROUP_SIZE, weight / GROUP_SIZE));

    if(WaveIsFirstLane())
    {
        gs_luminanceSum[threadIdx / WaveGetLaneCount()] = waveSumLuminance;
    }

    GroupMemoryBarrierWithGroupSync();

    float4 fetchedLuminance = 0.f;
    if(threadIdx < WAVES_NUM)
    {
        fetchedLuminance = gs_luminanceSum[threadIdx];
    }

    const float4 finalLuminance = WaveActiveSum(float4(fetchedLuminance));

    if(threadIdx == 0u)
    {
        u_rwProbe[uint2(0, 0)] = finalLuminance.xyz / finalLuminance.w;
    }
}
