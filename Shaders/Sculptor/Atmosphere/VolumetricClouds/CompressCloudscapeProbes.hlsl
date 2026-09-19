#include "SculptorShader.hlsli"

[[shader_params(CompressCloudscapeProbesConsts, PARAMS_COMPRESS_CLOUDSCAPE_PROBES)]]
[[shader_params(CloudscapeConstants, PARAMS_CLOUDSCAPE)]]

#include "Utils/Packing.hlsli"
#include "Atmosphere/VolumetricClouds/Cloudscape.hlsli"

struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


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

    for(uint rayIdxOffset = 0u; rayIdxOffset < PARAMS_COMPRESS_CLOUDSCAPE_PROBES->constants.raysPerProbe; rayIdxOffset += WaveGetLaneCount())
    {
        const uint localRayIdx = rayIdxOffset + WaveGetLaneIndex();
	    const float3 rayDir = ComputeCloudscapeProbeRayDirection(localRayIdx, PARAMS_COMPRESS_CLOUDSCAPE_PROBES->constants.raysPerProbe);
        const float4 traceResult = PARAMS_COMPRESS_CLOUDSCAPE_PROBES->traceResult.Load(uint3(localRayIdx, updatedProbeIdx, 0u));

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

    const uint2 probesNum = PARAMS_CLOUDSCAPE->probesNum;

#if FULL_UPDATE
    const uint2 probeCoords = uint2(updatedProbeIdx % probesNum.x, updatedProbeIdx / probesNum.x);
#else
    const uint2 probeCoords = PARAMS_COMPRESS_CLOUDSCAPE_PROBES->constants.probesToUpdate[updatedProbeIdx].xy;
#endif // FULL_UPDATE


    const uint2 probeDataCoords = probeCoords * PARAMS_COMPRESS_CLOUDSCAPE_PROBES->constants.compressedProbeDataRes;

    PARAMS_COMPRESS_CLOUDSCAPE_PROBES->rwCompressedProbes[probeDataCoords + coords] = traceRes;
}