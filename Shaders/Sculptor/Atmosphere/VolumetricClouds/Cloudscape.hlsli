#ifndef CLOUDSCAPE_HLSLI
#define CLOUDSCAPE_HLSLI

#include "Utils/Packing.hlsli"

float3 ComputeCloudscapeProbeRayDirection(in uint rayIdx, in uint raysNum)
{
	float3 direction = FibbonaciSphereDistribution(rayIdx, raysNum);
	direction.z = abs(direction.z);
    return direction;
}

float3 GetCloudscapeProbeLocation(CloudscapeConstants cloudscape, uint2 probeCoords)
{
    return float3(cloudscape.probesOrigin + cloudscape.probesSpacing * probeCoords, cloudscape.probesHeight);
}

#ifdef DS_CloudscapeProbesDS

struct CloudscapeSample
{
    float3 inScattering;
    float  transmittance;
};

CloudscapeSample SampleCloudscapeProbe(uint2 probeCoords, in float2 octUV)
{
    probeCoords = min(probeCoords, u_cloudscapeConstants.probesNum - 1u);
    const float2 uv = probeCoords * u_cloudscapeConstants.uvPerProbe + u_cloudscapeConstants.uvBorder + u_cloudscapeConstants.uvPerProbeNoBorders * octUV;

    const float4 probeData = u_cloudscapeProbes.SampleLevel(u_cloudscapeProbesSampler, uv, 0.f);

    CloudscapeSample res;
    res.inScattering  = probeData.xyz;
    res.transmittance = probeData.w;

    return res;
}

CloudscapeSample SampleCloudscape(in float3 location, in float3 direction)
{
    CloudscapeSample result;

    if(direction.z <= 0.f)
    {
        result.inScattering = 0.f;
        result.transmittance = 1.f;
        return result;
    }

    result.inScattering = 0.f;
    result.transmittance = 0.f;

    const float2 probeCoords = (location.xy - u_cloudscapeConstants.probesOrigin) * u_cloudscapeConstants.rcpProbesSpacing;

    const float2 fracCoords = frac(probeCoords);

    const uint2 baseProbeCoords = uint2(max(probeCoords, 0.f));

    const float2 octUV = OctahedronEncodeHemisphereNormal(direction);

    for(uint probeIdx = 0u; probeIdx < 4u; ++probeIdx)
    {
        const uint2 offsetCoords = uint2((probeIdx & 1u), (probeIdx & 2u) >> 1u);

        const uint2 currentProbeCoords = baseProbeCoords + offsetCoords;

        const CloudscapeSample currentSample = SampleCloudscapeProbe(currentProbeCoords, octUV);
        const float weight = ((offsetCoords.x == 1u) ? fracCoords.x : 1.f - fracCoords.x) * ((offsetCoords.y == 1u) ? fracCoords.y : 1.f - fracCoords.y);

        result.inScattering  += currentSample.inScattering * weight;
        result.transmittance += currentSample.transmittance * weight;
    }

    return result;
}


CloudscapeSample SampleHighResCloudscape(in float3 direction)
{
    CloudscapeSample result;

    if(direction.z <= 0.f)
    {
        result.inScattering = 0.f;
        result.transmittance = 1.f;
        return result;
    }

    const float2 octUV = OctahedronEncodeHemisphereNormal(direction);

    const float4 probesData = u_cloudscapeHighResProbe.SampleLevel(u_cloudscapeProbesSampler, octUV, 0.f);

    result.inScattering  = probesData.xyz;
    result.transmittance = probesData.w;

    return result;
}

#endif // DS_CloudscapeProbesDS

#endif // CLOUDSCAPE_HLSLI