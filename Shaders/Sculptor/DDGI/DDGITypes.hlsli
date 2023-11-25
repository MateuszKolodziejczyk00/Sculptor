#ifndef DDGI_TYPES_HLSLI
#define DDGI_TYPES_HLSLI

float3 GetProbeRayDirection(in uint rayIdx, in uint raysNum)
{
    return FibbonaciSphereDistribution(rayIdx, raysNum);
}

float2 GetProbeOctCoords(in float3 dir)
{
    return OctahedronEncode(dir);
}


float3 GetProbeOctDirection(in float2 coords)
{
    return OctahedronDecode(coords);
}


uint3 ComputeUpdatedProbeCoords(in uint probeIdx, in uint3 probesToUpdateCoords, in uint3 probesToUpdateCount)
{
    const uint probesPerPlane = probesToUpdateCount.x * probesToUpdateCount.y;

    const uint z = probeIdx / probesPerPlane;
    const uint y = (probeIdx - z * probesPerPlane) / probesToUpdateCount.x;
    const uint x = probeIdx - z * probesPerPlane - y * probesToUpdateCount.x;

    return uint3(x, y, z) + probesToUpdateCoords;
}


float3 GetProbeWorldLocation(in const DDGIGPUParams ddgiParams, in uint3 probedCoords)
{
    return ddgiParams.probesOriginWorldLocation + float3(probedCoords) * ddgiParams.probesSpacing;
}


uint3 ComputeProbeWrappedCoords(in const DDGIGPUParams ddgiParams, in uint3 probeCoords)
{
    const int3 wrappedCoords = probeCoords - ddgiParams.probesWrapCoords;

    // add 1 because we need  '<' instead of '<='
    const uint3 wrapDelta = (wrappedCoords < 0) * ddgiParams.probesVolumeResolution;

    return wrappedCoords + wrapDelta;
}


uint3 GetProbeCoordsAtWorldLocation(in const DDGIGPUParams ddgiParams, in float3 worldLocation)
{
    return (worldLocation - ddgiParams.probesOriginWorldLocation) * ddgiParams.rcpProbesSpacing;
}


uint3 OffsetProbe(in const DDGIGPUParams ddgiParams, in uint3 probeCoords, in int3 probeOffset)
{
    const int3 newCoords = int3(probeCoords) + probeOffset;
    return clamp(newCoords, 0, int3(ddgiParams.probesVolumeResolution) - 1);
}


uint2 ComputeProbeDataCoords(in const DDGIGPUParams ddgiParams, in uint3 probeWrappedCoords)
{
    const uint x = probeWrappedCoords.x + probeWrappedCoords.z * ddgiParams.probesVolumeResolution.x;
    const uint y = probeWrappedCoords.y;
    return uint2(x, y);
}


uint2 ComputeProbeIlluminanceDataOffset(in const DDGIGPUParams ddgiParams, in uint3 probeWrappedCoords)
{
    const uint2 probeDataCoords = ComputeProbeDataCoords(ddgiParams, probeWrappedCoords);
    return probeDataCoords * ddgiParams.probeIlluminanceDataWithBorderRes;
}


uint2 ComputeProbeHitDistanceDataOffset(in const DDGIGPUParams ddgiParams, in uint3 probeWrappedCoords)
{
    const uint2 probeDataCoords = ComputeProbeDataCoords(ddgiParams, probeWrappedCoords);
    return probeDataCoords * ddgiParams.probeHitDistanceDataWithBorderRes;
}


float3 SampleProbeIlluminance(in const DDGIGPUParams ddgiParams, Texture2D<float3> probesIlluminanceTexture, SamplerState illuminanceSampler, in uint3 probeWrappedCoords, float2 octahedronUV)
{
    const uint2 probeDataCoords = ComputeProbeDataCoords(ddgiParams, probeWrappedCoords);
    
    // Probe data begin UV
    float2 uv = probeDataCoords * ddgiParams.probesIlluminanceTextureUVDeltaPerProbe;
    
    // Add Border
    uv += ddgiParams.probesIlluminanceTexturePixelSize;

    // add octahedron UV
    uv += octahedronUV * ddgiParams.probesIlluminanceTextureUVPerProbeNoBorder;

    return probesIlluminanceTexture.SampleLevel(illuminanceSampler, uv, 0);
}


float2 SampleProbeHitDistance(in const DDGIGPUParams ddgiParams, Texture2D<float2> probesHitDistanceTexture, SamplerState distancesSampler, in uint3 probeWrappedCoords, float2 octahedronUV)
{
    const uint2 probeDataCoords = ComputeProbeDataCoords(ddgiParams, probeWrappedCoords);
    
    // Probe data begin UV
    float2 uv = probeDataCoords * ddgiParams.probesHitDistanceUVDeltaPerProbe;
    
    // Add Border
    uv += ddgiParams.probesHitDistanceTexturePixelSize;

    // add octahedron UV
    uv += octahedronUV * ddgiParams.probesHitDistanceTextureUVPerProbeNoBorder;
    
    return probesHitDistanceTexture.SampleLevel(distancesSampler, uv, 0);
}


bool IsInsideDDGIVolume(in const DDGIGPUParams ddgiParams, in float3 worldLocation)
{
    return all(worldLocation >= ddgiParams.probesOriginWorldLocation - ddgiParams.probesSpacing) && all(worldLocation <= ddgiParams.probesEndWorldLocation + ddgiParams.probesSpacing);
}


struct DDGISampleParams
{
    float3 worldLocation;
    float3 surfaceNormal;
    float3 viewNormal;

    float  minVisibility;

    float3 sampleDirection;
    float  sampleLocationBiasMultiplier;
};


DDGISampleParams CreateDDGISampleParams(in float3 worldLocation, in float3 surfaceNormal, in float3 viewNormal)
{
    DDGISampleParams params;
    params.worldLocation                = worldLocation;
    params.surfaceNormal                = surfaceNormal;
    params.viewNormal                   = viewNormal;
    params.sampleDirection              = surfaceNormal;
    params.sampleLocationBiasMultiplier = 1.0f;
    params.minVisibility                = 0.0f;
    return params;
}


float3 DDGIGetSampleLocation(in const DDGIGPUParams ddgiParams, in DDGISampleParams sampleParams)
{
    const float3 biasVector = (sampleParams.surfaceNormal * 0.3f + sampleParams.viewNormal * 0.7f) * 0.75f * ddgiParams.probesSpacing * 0.75f * sampleParams.sampleLocationBiasMultiplier;
    return sampleParams.worldLocation + biasVector;
}


float3 DDGISampleLuminance(in const DDGIGPUParams ddgiParams,
                           in Texture2D<float3> probesIlluminanceTexture,
                           in SamplerState illuminanceSampler,
                           in Texture2D<float2> probesHitDistanceTexture,
                           in SamplerState distancesSampler,
                           in DDGISampleParams sampleParams)
{
    if(!IsInsideDDGIVolume(ddgiParams, sampleParams.worldLocation))
    {
        return float3(0, 0, 0);
    }
    
    const float3 biasedWorldLocation = DDGIGetSampleLocation(ddgiParams, sampleParams);

    const uint3 baseProbeCoords = GetProbeCoordsAtWorldLocation(ddgiParams, biasedWorldLocation);
    const float3 baseProbeWorldLocation = GetProbeWorldLocation(ddgiParams, baseProbeCoords);

    const float3 baseProbeDistAlpha = saturate((biasedWorldLocation - baseProbeWorldLocation) * ddgiParams.rcpProbesSpacing);

    float3 luminanceSum = 0.f;
    float weightSum = 0.f;

    const float rcpMaxVisibility = rcp(1.f / sampleParams.minVisibility);
    
    for (int i = 0; i < 8; ++i)
    {
        const int3 probeOffset = int3(i, i >> 1, i >> 2) & int3(1, 1, 1);
        const uint3 probeCoords = OffsetProbe(ddgiParams, baseProbeCoords, probeOffset);

        const uint3 probeWrappedCoords = ComputeProbeWrappedCoords(ddgiParams, probeCoords);

        const float3 probeWorldLocation = GetProbeWorldLocation(ddgiParams, probeCoords);

        const float3 trilinear = max(lerp(1.f - baseProbeDistAlpha, baseProbeDistAlpha, probeOffset), 0.001f);
        const float trilinearWeight = trilinear.x * trilinear.y * trilinear.z;
        float weight = 1.f;
        
        const float3 dirToProbe = normalize(probeWorldLocation - sampleParams.worldLocation);

        const float probeDirDotNormal = saturate(dot(dirToProbe, sampleParams.surfaceNormal) * 0.5f + 0.5f);
        weight *= Pow2(probeDirDotNormal);
        
        float3 probeToBiasedPointDir = biasedWorldLocation - probeWorldLocation;
        const float distToBiasedPoint = length(probeToBiasedPointDir);
        probeToBiasedPointDir /= distToBiasedPoint;

        const float2 hitDistOctCoords = GetProbeOctCoords(probeToBiasedPointDir);
        
        const float2 hitDistances = SampleProbeHitDistance(ddgiParams, probesHitDistanceTexture, distancesSampler, probeWrappedCoords, hitDistOctCoords);

        if (hitDistances.x < 0.f)
        {
            // probe is inside geometry
            weight = 0.0001f;
        }
        else if (distToBiasedPoint > hitDistances.x)
        {
            const float variance = abs(Pow2(hitDistances.x) - hitDistances.y);

            const float distDiff = distToBiasedPoint - hitDistances.x;
            float chebyshevWeight = (variance / (variance + Pow2(distDiff)));

            chebyshevWeight = max(Pow3(chebyshevWeight) - 0.2f, 0.00f);

            chebyshevWeight = saturate(chebyshevWeight - sampleParams.minVisibility) * rcpMaxVisibility;

            weight *= chebyshevWeight;
        }

        weight = max(weight, 0.00001f);
        
        if(weight < 0.2f)
        {
            weight *= Pow2(weight) / Pow2(0.2f);
        }

        weight *= trilinearWeight;
        
        const float2 luminanceOctCoords = GetProbeOctCoords(sampleParams.sampleDirection);

        float3 luminance = SampleProbeIlluminance(ddgiParams, probesIlluminanceTexture, illuminanceSampler, probeWrappedCoords, luminanceOctCoords);

        luminance = pow(luminance, ddgiParams.probeIlluminanceEncodingGamma * 0.5f);

        luminanceSum += luminance * weight;
        weightSum += weight;
    }

    float3 luminance = luminanceSum / weightSum;

    luminance = Pow2(luminance);

    return luminance;
}


float3 DDGISampleIlluminance(in const DDGIGPUParams ddgiParams,
                             in Texture2D<float3> probesIlluminanceTexture,
                             in SamplerState illuminanceSampler,
                             in Texture2D<float2> probesHitDistanceTexture,
                             in SamplerState distancesSampler,
                             in DDGISampleParams sampleParams)
{
    const float3 luminance = DDGISampleLuminance(ddgiParams,
                                                 probesIlluminanceTexture,
                                                 illuminanceSampler,
                                                 probesHitDistanceTexture,
                                                 distancesSampler,
                                                 sampleParams);

    return luminance * 2.f * PI; // multiply by integration domain area
}

#endif // DDGI_TYPES_HLSLI
