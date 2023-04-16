
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


uint3 ComputeUpdatedProbeCoords(uint probeIdx, uint3 probesToUpdateCoords, uint3 probesToUpdateCount)
{
    const uint probesPerPlane = probesToUpdateCount.x * probesToUpdateCount.y;

    const uint z = probeIdx / probesPerPlane;
    const uint y = (probeIdx - z * probesPerPlane) / probesToUpdateCount.x;
    const uint x = probeIdx - z * probesPerPlane - y * probesToUpdateCount.x;

    return uint3(x, y, z) + probesToUpdateCoords;
}


float3 GetProbeWorldLocation(in const DDGIGPUParams ddgiParams, in uint3 probeWrappedCoords)
{
	return ddgiParams.probesOriginWorldLocation + float3(probeWrappedCoords) * ddgiParams.probesSpacing;
}


uint3 ComputeProbeWrappedCoords(in const DDGIGPUParams ddgiParams, in uint3 probeCoords)
{
	const uint3 wrappedCoords = probeCoords - ddgiParams.probesWrapCoords;

	// add 1 because we need  '<' instead of '<='
	const uint3 wrapDelta = step(probeCoords + 1u, ddgiParams.probesWrapCoords) * ddgiParams.probesVolumeResolution;

	return wrappedCoords + wrapDelta;
}


uint2 ComputeProbeDataCoords(in const DDGIGPUParams ddgiParams, in uint3 probeWrappedCoords)
{
	const uint x = probeWrappedCoords.x + probeWrappedCoords.z * ddgiParams.probesVolumeResolution.x;
	const uint y = probeWrappedCoords.y;
	return uint2(x, y);
}


uint2 ComputeProbeIrradianceDataOffset(in const DDGIGPUParams ddgiParams, in uint3 probeWrappedCoords)
{
	const uint2 probeDataCoords = ComputeProbeDataCoords(ddgiParams, probeWrappedCoords);
	return probeDataCoords * ddgiParams.probeIrradianceDataWithBorderRes;
}


uint2 ComputeProbeHitDistanceDataOffset(in const DDGIGPUParams ddgiParams, in uint3 probeWrappedCoords)
{
	const uint2 probeDataCoords = ComputeProbeDataCoords(ddgiParams, probeWrappedCoords);
	return probeDataCoords * ddgiParams.probeHitDistanceDataWithBorderRes;
}


float3 SampleProbeIrradiance(in const DDGIGPUParams ddgiParams, Texture2D<float3> probesIrradianceTexture, SamplerState irradianceSampler, in uint3 probeWrappedCoords, float2 octahedronUV)
{
	const uint2 probeDataCoords = ComputeProbeDataCoords(ddgiParams, probeWrappedCoords);
	
	// Probe data begin UV
    float2 uv = probeDataCoords * ddgiParams.probesIrradianceTextureUVDeltaPerProbe;
	
	// Add Border
    uv += ddgiParams.probesIrradianceTexturePixelSize;

	// add octahedron UV
    uv += octahedronUV * ddgiParams.probesIrradianceTextureUVPerProbeNoBorder;

	return probesIrradianceTexture.SampleLevel(irradianceSampler, uv, 0);
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