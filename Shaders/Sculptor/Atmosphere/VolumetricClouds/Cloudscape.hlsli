#ifndef CLOUDSCAPE_HLSLI
#define CLOUDSCAPE_HLSLI

#include "Utils/Packing.hlsli"
#include "Utils/Shapes.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"

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

#ifdef PARAM_CloudscapeProbesParams

float3 SampleCloudscapeIrradiance(in float3 location)
{
	const float2 probeCoords = (location.xy - PARAM_CloudscapeProbesParams->cloudscapeConstants->probesOrigin) * PARAM_CloudscapeProbesParams->cloudscapeConstants->rcpProbesSpacing;
	const float2 uv = probeCoords * PARAM_CloudscapeProbesParams->cloudscapeConstants->uvPerProbe;
	return PARAM_CloudscapeProbesParams->cloudscapeIrradianceProbes.SampleLevel(BindlessSamplers::LinearClampEdge(), uv, 0.f).xyz;
}

float3 SampleCloudscapeInScattering(in float3 location, in float3 direction)
{
	const float3 cloudscapeIrradiance = SampleCloudscapeIrradiance(location);

	float2 skyInScatteringLUTUV;
	skyInScatteringLUTUV.x = saturate(location.z / HEIGHT_FOG_MAX_HEIGHT);
	skyInScatteringLUTUV.y =  direction.z * 0.5f + 0.5f;
	const float3 skyInScattering = PARAM_CloudscapeProbesParams->skyInScatteringLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), skyInScatteringLUTUV, 0);

	return cloudscapeIrradiance * skyInScattering;
}

struct CloudscapeSample
{
    float3 inScattering;
    float  transmittance;
};


CloudscapeSample SampleCloudscapeProbe(uint2 probeCoords, in float2 octUV)
{
    probeCoords = min(probeCoords, PARAM_CloudscapeProbesParams->cloudscapeConstants->probesNum - 1u);
    const float2 uv = probeCoords * PARAM_CloudscapeProbesParams->cloudscapeConstants->uvPerProbe + PARAM_CloudscapeProbesParams->cloudscapeConstants->uvBorder + PARAM_CloudscapeProbesParams->cloudscapeConstants->uvPerProbeNoBorders * octUV;

    const float4 probeData = PARAM_CloudscapeProbesParams->cloudscapeProbes.SampleLevel(BindlessSamplers::LinearClampEdge(), uv, 0.f);

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

    const float2 probeCoords = (location.xy - PARAM_CloudscapeProbesParams->cloudscapeConstants->probesOrigin) * PARAM_CloudscapeProbesParams->cloudscapeConstants->rcpProbesSpacing;

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

    const float4 probesData = PARAM_CloudscapeProbesParams->cloudscapeHighResProbe.SampleLevel(BindlessSamplers::LinearClampEdge(), octUV, 0.f);

    result.inScattering  = probesData.xyz;
    result.transmittance = probesData.w;

    return result;
}


CloudscapeSample SampleHighResCloudscape(in float3 location, in float3 direction)
{
    CloudscapeSample result;

    if(direction.z <= 0.f)
    {
        result.inScattering = 0.f;
        result.transmittance = 1.f;
        return result;
    }

	const float3 cloudsAtmosphereCenter = PARAM_CloudscapeProbesParams->cloudscapeConstants->cloudsAtmosphereCenter;
	const float cloudsAtmosphereInnerRadius = PARAM_CloudscapeProbesParams->cloudscapeConstants->cloudsAtmosphereInnerRadius;

	const Sphere cloudscapeSphere = Sphere::Create(cloudsAtmosphereCenter, cloudsAtmosphereInnerRadius);
	const Ray ray = Ray::Create(location, direction);

	const IntersectionResult intersection = ray.IntersectSphere(cloudscapeSphere);
	if (intersection.IsValid())
	{
		const float3 rayEnd = location + direction * intersection.GetTime();
		return SampleHighResCloudscape(normalize(rayEnd));
	}

	result.inScattering = 0.f;
	result.transmittance = 1.f;
	return result;
}

#endif // PARAM_CloudscapeProbesParams

#endif // CLOUDSCAPE_HLSLI
