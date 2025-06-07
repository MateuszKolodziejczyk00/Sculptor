#include "SculptorShader.hlsli"

[[descriptor_set(CompressCloudscapeProbesDS, 0)]]
[[descriptor_set(CloudscapeDS, 1)]]

#include "Utils/Packing.hlsli"
#include "Atmosphere/VolumetricClouds/Cloudscape.hlsli"

struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


#define RAYS_NUM 1024u


[numthreads(1024, 1, 1)]
void CompressCloudscapeProbesCS(CS_INPUT input)
{
    const uint2 coords = uint2(input.globalID.x / 32u, input.globalID.x & 31u);

    const float2 probeUV = (coords + 0.5f) / 32.f;

    const float3 direction = OctahedronDecodeHemisphereNormal(probeUV);

    const uint updatedProbeIdx = input.globalID.y;

    float4 traceResSum = 0.f;
    float4 traceResSumNoWeight = 0.f;
    float4 weightSum = 0.f;

    for(uint rayIdx = 0u; rayIdx < RAYS_NUM; ++rayIdx)
    {
        const float4 traceResult = u_traceResult.Load(uint3(rayIdx, updatedProbeIdx, 0u));
	    const float3 rayDir = ComputeCloudscapeProbeRayDirection(rayIdx, u_constants.raysPerProbe);

        const float weight = pow(max(dot(direction, rayDir), 0.0f), 64.f);

        traceResSum += traceResult * weight;
        weightSum += weight;

        traceResSumNoWeight += traceResult;
    }

    const float4 traceRes = traceResSum / weightSum;

    const uint2 probeCoords = u_constants.probesToUpdate[updatedProbeIdx].xy;

    const uint2 probeDataCoords = probeCoords * u_constants.compressedProbeDataRes;

    u_rwCompressedProbes[probeDataCoords + coords] = traceRes;

    if(input.globalID.x == 0u)
    {
        u_rwCompressedSimpleProbes[probeCoords] = (traceResSumNoWeight / RAYS_NUM);
    }
}