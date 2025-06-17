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
    const uint2 coords = uint2(input.globalID.x & 31u, input.globalID.x >> 5u);

    const float2 probeUV = (coords + 0.5f) / 32.f;

    const float3 direction = OctahedronDecodeHemisphereNormal(probeUV);

    const uint updatedProbeIdx = input.globalID.y;

    float4 traceResSum = 0.f;
    float4 traceResSumNoWeight = 0.f;
    float4 weightSum = 0.f;

    for(uint rayIdxOffset = 0u; rayIdxOffset < RAYS_NUM; rayIdxOffset += WaveGetLaneCount())
    {
        const uint localRayIdx = rayIdxOffset + WaveGetLaneIndex();
	    const float3 rayDir = ComputeCloudscapeProbeRayDirection(localRayIdx, u_constants.raysPerProbe);
        const float4 traceResult = u_traceResult.Load(uint3(localRayIdx, updatedProbeIdx, 0u));

        [unroll]
        for(uint localIdx = 0u; localIdx < WaveGetLaneCount(); ++localIdx)
        {
            const float4 currTraceResult = WaveReadLaneAt(traceResult, localIdx);
	        const float3 currRayDir      = WaveReadLaneAt(rayDir, localIdx);

            const float weight = pow(max(dot(direction, currRayDir), 0.0f), 64.f);

            traceResSum += currTraceResult * weight;
            weightSum += weight;
        }
    }

    const float4 traceRes = traceResSum / weightSum;

    const uint2 probeCoords = u_constants.probesToUpdate[updatedProbeIdx].xy;

    const uint2 probeDataCoords = probeCoords * u_constants.compressedProbeDataRes;

    u_rwCompressedProbes[probeDataCoords + coords] = traceRes;
}