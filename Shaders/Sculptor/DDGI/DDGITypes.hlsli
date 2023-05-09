
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


float3 SampleIrradiance(in const DDGIGPUParams ddgiParams,
						in Texture2D<float3> probesIrradianceTexture,
						in SamplerState irradianceSampler, 
						in Texture2D<float2> probesHitDistanceTexture, 
						in SamplerState distancesSampler,
						in float3 worldLocation,
						in float3 surfaceNormal,
						in float3 viewNormal)
{
	const float3 biasVector = (surfaceNormal * 0.2f + viewNormal * 0.8f) * 0.75f * ddgiParams.probesSpacing * 0.4f;
	const float3 biasedWorldLocation = worldLocation + biasVector;

	const uint3 baseProbeCoords = GetProbeCoordsAtWorldLocation(ddgiParams, biasedWorldLocation);
	const float3 baseProbeWorldLocation = GetProbeWorldLocation(ddgiParams, baseProbeCoords);

	const float3 baseProbeDistAlpha = saturate(biasedWorldLocation - baseProbeWorldLocation * ddgiParams.rcpProbesSpacing);

    float3 irradianceSum = 0.f;
    float weightSum = 0.f;
	
    for (int i = 0; i < 8; ++i)
    {
        const int3 probeOffset = int3(i, i >> 1, i >> 2) & 1;
		const uint3 probeCoords = OffsetProbe(ddgiParams, baseProbeCoords, probeOffset);

		const uint3 probeWrappedCoords = ComputeProbeWrappedCoords(ddgiParams, probeCoords);

		const float3 probeWorldLocation = GetProbeWorldLocation(ddgiParams, probeCoords);

        const float3 trilinearWeight = lerp(1.f - baseProbeDistAlpha, baseProbeDistAlpha, probeOffset);
        float weight = 1.f;
		
		const float3 dirToProbe = normalize(probeWorldLocation - worldLocation);

		const float probeDirDotNormal = saturate(dot(dirToProbe, surfaceNormal) * 0.5f + 0.5f);
        weight *= Pow2(probeDirDotNormal) + 0.2f;
		
		float3 probeToBiasedPointDir = biasedWorldLocation - probeWorldLocation;
		const float distToBiasedPoint = length(probeToBiasedPointDir);
        probeToBiasedPointDir /= distToBiasedPoint;

        const float2 hitDistOctCoords = GetProbeOctCoords(probeToBiasedPointDir);
		
		const float2 hitDistances = SampleProbeHitDistance(ddgiParams, probesHitDistanceTexture, distancesSampler, probeWrappedCoords, hitDistOctCoords);

		if(distToBiasedPoint > hitDistances.x)
        {
            const float variance = abs(Pow2(hitDistances.x) - hitDistances.y);

			const float distDiff = distToBiasedPoint - hitDistances.x;
			float chebyshevWeight = (variance / (variance + Pow2(distDiff)));

            chebyshevWeight = max(Pow3(chebyshevWeight), 0.05f);

			weight *= chebyshevWeight;
        }

		weight = max(weight, 0.00001f);
		
		if(weight < 0.2f)
        {
            weight *= Pow2(weight) / (1.f / Pow2(0.2f));
        }
		
		const float2 irradianceOctCoords = GetProbeOctCoords(surfaceNormal);

		float3 irradiance = SampleProbeIrradiance(ddgiParams, probesIrradianceTexture, irradianceSampler, probeWrappedCoords, irradianceOctCoords);

		// perceptual encoding
		irradiance = pow(irradiance, 2.5f);

        weight *= trilinearWeight.x * trilinearWeight.y * trilinearWeight.z + 0.001f;

        irradianceSum += irradiance * weight;
        weightSum += weight;
    }

	float3 irradiance = irradianceSum / weightSum;

	// perceptual encoding
	irradiance = Pow2(irradiance);

    irradiance = 0.5f * PI * irradiance * 0.95f;

    return irradiance;
}
