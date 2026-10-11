#include "SculptorShader.hlsli"

[[shader_params(CloudscapeConstants, CLOUDSCAPE)]]
[[shader_params(CompressCloudscapeProbesConsts, CONSTS)]]

#include "Utils/Packing.hlsli"
#include "Atmosphere/VolumetricClouds/Cloudscape.hlsli"
#include "Atmosphere/Atmosphere.hlsli"

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

    const uint2 probesNum = CLOUDSCAPE->probesNum;

#if FULL_UPDATE
    const uint2 probeCoords = uint2(updatedProbeIdx % probesNum.x, updatedProbeIdx / probesNum.x);
#else
    const uint2 probeCoords = CONSTS->constants.probesToUpdate[updatedProbeIdx].xy;
#endif // FULL_UPDATE

    const float3 probeLocation = GetCloudscapeProbeLocation(*CLOUDSCAPE, probeCoords);
	const float3 probeLocationInAtmoshpere = GetLocationInAtmosphere(*CLOUDSCAPE->atmosphereConstants, probeLocation);

    float4 traceResSum = 0.f;
    float4 traceResSumNoWeight = 0.f;
    float4 weightSum = 0.f;

	float3 inScatteringSum = 0.f;

    for(uint rayIdxOffset = 0u; rayIdxOffset < CONSTS->constants.raysPerProbe; rayIdxOffset += WaveGetLaneCount())
    {
        const uint localRayIdx = rayIdxOffset + WaveGetLaneIndex();
	    const float3 rayDir = ComputeCloudscapeProbeRayDirection(localRayIdx, CONSTS->constants.raysPerProbe);
        const float4 traceResult = CONSTS->traceResult.Load(uint3(localRayIdx, updatedProbeIdx, 0u));

        [unroll]
        for(uint localIdx = 0u; localIdx < WaveGetLaneCount(); ++localIdx)
        {
            const float4 currTraceResult = WaveReadLaneAt(traceResult, localIdx);
	        const float3 currRayDir      = WaveReadLaneAt(rayDir, localIdx);

			const float3 skyLuminance = GetLuminanceFromSkyViewLUT(*CLOUDSCAPE->atmosphereConstants, CONSTS->skyViewLUT, BindlessSamplers::LinearClampEdge(), probeLocationInAtmoshpere, currRayDir);
			inScatteringSum += currTraceResult.rgb + currTraceResult.a * skyLuminance;

            const float weight = pow(max(dot(direction, currRayDir), 0.0f), 64.f);

            traceResSum += currTraceResult * weight;
            weightSum += weight;
        }
    }

	// We need to multiply by 0.5 because we are only sampling the upper hemisphere, but the scattering is over the entire sphere
	inScatteringSum *= 0.5f;

    const float4 traceRes = traceResSum / weightSum;

    const uint2 probeDataCoords = probeCoords * CONSTS->constants.compressedProbeDataRes;

    CONSTS->rwCompressedProbes[probeDataCoords + coords] = traceRes;

	if (input.globalID.x == 0u)
	{
		const float3 irradiance = (inScatteringSum / CONSTS->constants.raysPerProbe) * (PI * 4.f);

		CONSTS->rwCompressedIrradianceProbes[probeCoords] = float4(irradiance, 1.f);
	}
}
