#ifndef DDGI_TYPES_HLSLI
#define DDGI_TYPES_HLSLI

#include "Utils/Packing.hlsli"

[[shader_struct(DDGIVolumeGPUParams)]]


float3 GetProbeRayDirection(in uint rayIdx, in uint raysNum)
{
	return FibbonaciSphereDistribution(rayIdx, raysNum);
}

float2 GetProbeOctCoords(in float3 dir)
{
	return OctahedronEncodeNormal(dir);
}


float3 GetProbeOctDirection(in float2 coords)
{
	return OctahedronDecodeNormal(coords);
}


uint3 ComputeUpdatedProbeCoords(in uint probeIdx, in uint3 probesToUpdateCoords, in uint3 probesToUpdateCount)
{
	const uint probesPerPlane = probesToUpdateCount.x * probesToUpdateCount.y;

	const uint z = probeIdx / probesPerPlane;
	const uint y = (probeIdx - z * probesPerPlane) / probesToUpdateCount.x;
	const uint x = probeIdx - z * probesPerPlane - y * probesToUpdateCount.x;

	return uint3(x, y, z) + probesToUpdateCoords;
}


float3 GetProbeWorldLocation(in const DDGIVolumeGPUParams volumeParams, in uint3 probedCoords)
{
	return volumeParams.probesOriginWorldLocation + float3(probedCoords) * volumeParams.probesSpacing;
}


uint3 ComputeProbeWrappedCoords(in const DDGIVolumeGPUParams volumeParams, in uint3 probeCoords)
{
	const int3 wrappedCoords = probeCoords + volumeParams.probesWrapCoords;

	const int3 wrapDelta = (wrappedCoords >= volumeParams.probesVolumeResolution) * volumeParams.probesVolumeResolution;

	return wrappedCoords - wrapDelta;
}


uint3 GetProbeCoordsAtWorldLocation(in const DDGIVolumeGPUParams volumeParams, in float3 worldLocation)
{
	return min((worldLocation - volumeParams.probesOriginWorldLocation) * volumeParams.rcpProbesSpacing, volumeParams.probesVolumeResolution - 2);
}


uint3 OffsetProbe(in const DDGIVolumeGPUParams volumeParams, in uint3 probeCoords, in int3 probeOffset)
{
	const int3 newCoords = int3(probeCoords) + probeOffset;
	return clamp(newCoords, 0, int3(volumeParams.probesVolumeResolution) - 1);
}


struct DDGIProbeDataCoords
{
	uint2 textureLocalCoords;
	uint textureIdx;
};


DDGIProbeDataCoords ComputeProbeDataCoords(in const DDGIVolumeGPUParams volumeParams, in uint3 probeWrappedCoords)
{
	const uint x = probeWrappedCoords.x;
	const uint y = probeWrappedCoords.y;
	DDGIProbeDataCoords coords;
	coords.textureLocalCoords = uint2(x, y);
	coords.textureIdx = probeWrappedCoords.z;
	return coords;
}


DDGIProbeDataCoords ComputeProbeIlluminanceDataOffset(in const DDGIVolumeGPUParams volumeParams, in uint3 probeWrappedCoords)
{
	DDGIProbeDataCoords probeDataCoords = ComputeProbeDataCoords(volumeParams, probeWrappedCoords);
	probeDataCoords.textureLocalCoords *= volumeParams.probeIlluminanceDataWithBorderRes;
	return probeDataCoords;
}


DDGIProbeDataCoords ComputeProbeHitDistanceDataOffset(in const DDGIVolumeGPUParams volumeParams, in uint3 probeWrappedCoords)
{
	DDGIProbeDataCoords probeDataCoords = ComputeProbeDataCoords(volumeParams, probeWrappedCoords);
	probeDataCoords.textureLocalCoords *= volumeParams.probeHitDistanceDataWithBorderRes;
	return probeDataCoords;
}


bool IsInsideDDGIVolume(in const DDGIVolumeGPUParams volumeParams, in float3 worldLocation)
{
	return all(worldLocation >= volumeParams.probesOriginWorldLocation) && all(worldLocation <= volumeParams.probesEndWorldLocation);
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


float3 ComputeDDGIBiasedSampleLocation(in const DDGIVolumeGPUParams volumeParams, in DDGISampleParams sampleParams)
{
	const float3 biasVector = normalize(sampleParams.surfaceNormal + sampleParams.viewNormal * 0.5f) * volumeParams.probesSpacing * sampleParams.sampleLocationBiasMultiplier;
	const float3 minWorldLocation = volumeParams.probesOriginWorldLocation + volumeParams.probesSpacing * 0.02f;
	const float3 maxWorldLocation = volumeParams.probesEndWorldLocation - volumeParams.probesSpacing * 0.02f;
	return clamp(sampleParams.worldLocation + biasVector, minWorldLocation, maxWorldLocation);
}

#ifdef DS_DDGISceneDS

float3 SampleProbeIlluminance(in const DDGIVolumeGPUParams volumeParams, SamplerState illuminanceSampler, in uint3 probeWrappedCoords, float2 octahedronUV)
{
	const DDGIProbeDataCoords probeDataCoords = ComputeProbeDataCoords(volumeParams, probeWrappedCoords);

	const Texture2D<float4> probesIlluminanceTexture = u_probesTextures2D[volumeParams.illuminanceTextureIdx + probeDataCoords.textureIdx];
	
	// Probe data begin UV
	float2 uv = probeDataCoords.textureLocalCoords * volumeParams.probesIlluminanceTextureUVDeltaPerProbe;
	
	// Add Border
	uv += volumeParams.probesIlluminanceTexturePixelSize;

	// add octahedron UV
	uv += octahedronUV * volumeParams.probesIlluminanceTextureUVPerProbeNoBorder;

	return probesIlluminanceTexture.SampleLevel(illuminanceSampler, uv, 0).rgb;
}


float2 SampleProbeHitDistance(in const DDGIVolumeGPUParams volumeParams, SamplerState distancesSampler, in uint3 probeWrappedCoords, float2 octahedronUV)
{
	const DDGIProbeDataCoords probeDataCoords = ComputeProbeDataCoords(volumeParams, probeWrappedCoords);

	Texture2D<float4> probesHitDistanceTexture = u_probesTextures2D[volumeParams.hitDistanceTextureIdx + probeDataCoords.textureIdx];

	// Probe data begin UV
	float2 uv = probeDataCoords.textureLocalCoords * volumeParams.probesHitDistanceUVDeltaPerProbe;
	
	// Add Border
	uv += volumeParams.probesHitDistanceTexturePixelSize;

	// add octahedron UV
	uv += octahedronUV * volumeParams.probesHitDistanceTextureUVPerProbeNoBorder;
	
	return probesHitDistanceTexture.SampleLevel(distancesSampler, uv, 0).xy;
}


struct DDGIVolumeSampleInfo
{
	uint2 volumeIndices;
	float2 weights;
};


float ComputeDDGIVolumeWeight(in const DDGIVolumeGPUParams volumeParams, in float3 location)
{
	float3 distToEdge = location - volumeParams.probesOriginWorldLocation;
	distToEdge = min(volumeParams.probesEndWorldLocation - location, distToEdge);

	distToEdge /= volumeParams.probesSpacing;
	return min(min(distToEdge.x, distToEdge.y), distToEdge.z);
}


DDGIVolumeSampleInfo GetDDGIVolumeSampleInfo(in float3 location)
{
	DDGIVolumeSampleInfo info;
	info.volumeIndices = IDX_NONE_32;
	info.weights = 0.0f;

	uint outIdx = 0;
	
	for(uint lodIdx = 0u; lodIdx < u_ddgiLODs.lodsNum && outIdx < 2u; ++lodIdx)
	{
		const DDGILODDefinition lodDef = u_ddgiLODs.lods[lodIdx];
		const DDGIVolumeGPUParams volume = u_volumesDef.volumes[lodDef.volumeIdx];

		if (IsInsideDDGIVolume(volume, location))
		{
			info.volumeIndices[outIdx] = lodDef.volumeIdx;
			info.volumeIndices[outIdx] = 0;
			info.weights[outIdx] = ComputeDDGIVolumeWeight(volume, location);
			++outIdx;
		}
	}

	return info;
}


float3 SampleProbeIlluminance(in const DDGIVolumeGPUParams volumeParams, in uint3 probeWrappedCoords, float2 octahedronUV)
{
	return SampleProbeIlluminance(volumeParams, u_probesDataSampler, probeWrappedCoords, octahedronUV);
}


float3 SampleProbeAverageLuminance(in const DDGIVolumeGPUParams volumeParams, in uint3 probeWrappedCoords)
{
	const Texture3D<float4> probesAverageLuminanceTexture = u_probesTextures3D[volumeParams.averageLuminanceTextureIdx];
	return probesAverageLuminanceTexture.Load(int4(probeWrappedCoords, 0)).rgb;
}


float2 SampleProbeHitDistance(in const DDGIVolumeGPUParams volumeParams, in uint3 probeWrappedCoords, float2 octahedronUV)
{
	return SampleProbeHitDistance(volumeParams, u_probesDataSampler, probeWrappedCoords, octahedronUV);
}


struct DDGISampleContext
{
	static DDGISampleContext Create()
	{
		DDGISampleContext context;
		return context;
	}

	float3 ComputeBiasedSampleLocation(in const DDGIVolumeGPUParams volumeParams, in DDGISampleParams sampleParams)
	{
		return ComputeDDGIBiasedSampleLocation(volumeParams, sampleParams);
	}
};


struct DDGISecondaryBounceSampleContext
{
	static DDGISecondaryBounceSampleContext Create(in float3 inPrevBounceLocation, in float3 inPrevBounceViewDirection)
	{
		DDGISecondaryBounceSampleContext context;
		context.prevBounceLocation      = inPrevBounceLocation;
		context.prevBounceViewDirection = inPrevBounceViewDirection;
		return context;
	}

	float3 ComputeBiasedSampleLocation(in const DDGIVolumeGPUParams volumeParams, in DDGISampleParams sampleParams)
	{
		const float bias = length(volumeParams.probesSpacing * sampleParams.sampleLocationBiasMultiplier);

		float3 toPrevBounce = prevBounceLocation - sampleParams.worldLocation;
		const float distToPrevBounce = length(toPrevBounce);
		toPrevBounce /= distToPrevBounce;

		if(bias > distToPrevBounce)
		{
			const float biasFromPrevBounce = bias - distToPrevBounce;
			return prevBounceLocation + prevBounceViewDirection * biasFromPrevBounce;
		}
		else
		{
			return sampleParams.worldLocation + toPrevBounce * bias;
		}
	}

	float3 prevBounceLocation;
	float3 prevBounceViewDirection;
};


template<typename TSampledDataType, typename TSampleCallback, typename TSampleContext>
TSampledDataType DDGISampleProbes(in const DDGIVolumeGPUParams volumeParams, in DDGISampleParams sampleParams, in TSampleContext sampleContext, in TSampleCallback sampleCallback)
{
	const float3 biasedWorldLocation = sampleContext.ComputeBiasedSampleLocation(volumeParams, sampleParams);

	const uint3 baseProbeCoords = GetProbeCoordsAtWorldLocation(volumeParams, biasedWorldLocation);
	const float3 baseProbeWorldLocation = GetProbeWorldLocation(volumeParams, baseProbeCoords);

	const float3 baseProbeDistAlpha = saturate((biasedWorldLocation - baseProbeWorldLocation) * volumeParams.rcpProbesSpacing);

	TSampledDataType sampledDataSum = 0.f;
	float weightSum = 0.f;

	const float rcpMaxVisibility = rcp(1.f - sampleParams.minVisibility);
	
	for (int i = 0; i < 8; ++i)
	{
		const int3 probeOffset = int3(i, i >> 1, i >> 2) & int3(1, 1, 1);
		const uint3 probeCoords = OffsetProbe(volumeParams, baseProbeCoords, probeOffset);

		const uint3 probeWrappedCoords = ComputeProbeWrappedCoords(volumeParams, probeCoords);

		const float3 probeWorldLocation = GetProbeWorldLocation(volumeParams, probeCoords);

		const float3 trilinear = max(lerp(1.f - baseProbeDistAlpha, baseProbeDistAlpha, probeOffset), 0.001f);
		const float trilinearWeight = trilinear.x * trilinear.y * trilinear.z;
		float weight = 1.f;
		
		const float3 dirToProbe = normalize(probeWorldLocation - sampleParams.worldLocation);

		const float probeDirDotNormal = saturate(dot(dirToProbe, sampleParams.surfaceNormal) * 0.5f + 0.5f);
		weight *= Pow2(probeDirDotNormal) + 0.2f;
		
		float3 probeToBiasedPointDir = biasedWorldLocation - probeWorldLocation;
		const float distToBiasedPoint = length(probeToBiasedPointDir);
		probeToBiasedPointDir /= distToBiasedPoint;

		const float2 hitDistOctCoords = GetProbeOctCoords(probeToBiasedPointDir);
		
		const float2 hitDistances = SampleProbeHitDistance(volumeParams, probeWrappedCoords, hitDistOctCoords);

		if (hitDistances.y < 0.f)
		{
			// probe is inside geometry
			weight = 0.0001f;
		}
		else if (distToBiasedPoint > hitDistances.x)
		{
			const float variance = abs(Pow2(hitDistances.x) - hitDistances.y);

			const float distDiff = distToBiasedPoint - hitDistances.x;
			float chebyshevWeight = (variance / (variance + Pow2(distDiff)));

			chebyshevWeight = max(Pow3(chebyshevWeight), 0.00f);

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

		const TSampledDataType probleSample = sampleCallback.Sample(volumeParams, probeWrappedCoords, luminanceOctCoords);
		if(!sampleCallback.IsValid(probleSample))
		{
			return sampleCallback.MakeInvalid();
		}

		sampledDataSum += probleSample * weight;
		weightSum += weight;
	}

	TSampledDataType result = sampledDataSum / weightSum;

	sampleCallback.PostProcess(result);

	return result;
}


class DDGILuminanceSampleCallback
{
	float3 Sample(in const DDGIVolumeGPUParams volumeParams, in uint3 probeWrappedCoords, in float2 octahedronUV)
	{
		float3 luminance = SampleProbeIlluminance(volumeParams, probeWrappedCoords, octahedronUV);
		if(IsValid(luminance))
		{
			luminance = pow(luminance, volumeParams.probeIlluminanceEncodingGamma * 0.5f);
		}
		return luminance;
	}

	bool IsValid(in float3 sampleValue)
	{
		return all(sampleValue >= 0.0f);
	}

	float3 MakeInvalid()
	{
		return -1.f;
	}

	void PostProcess(inout float3 result)
	{
		result = Pow2(result);
	}
};


template<typename TSampleContext>
float3 DDGISampleLuminanceInternal(in const DDGISampleParams sampleParams, in const TSampleContext context, in const DDGIVolumeGPUParams volumeParams)
{
	DDGILuminanceSampleCallback callback;
	return DDGISampleProbes<float3>(volumeParams, sampleParams, context, callback);
}


template<typename TSampleContext>
float3 DDGISampleLuminance(in DDGISampleParams sampleParams, in const TSampleContext context)
{
	const DDGIVolumeSampleInfo sampleInfo = GetDDGIVolumeSampleInfo(sampleParams.worldLocation);

	for(uint i = 0; i < 2; ++i)
	{
		if (sampleInfo.volumeIndices[i] != IDX_NONE_32)
		{
			const DDGIVolumeGPUParams volumeParams = u_volumesDef.volumes[sampleInfo.volumeIndices[i]];
			const float3 luminance = DDGISampleLuminanceInternal(sampleParams, context, volumeParams);
			if(all(luminance >= 0.0f))
			{
				return luminance;
			}
		}
	}

	return 0.f;
}


template<typename TSampleContext>
float3 DDGISampleLuminanceBlended(in DDGISampleParams sampleParams, in const TSampleContext context)
{
	const DDGIVolumeSampleInfo sampleInfo = GetDDGIVolumeSampleInfo(sampleParams.worldLocation);

	float3 luminance = 0.f;

	float weightSum = 0.f;

	for (uint i = 0; i < 2; ++i)
	{
		if (sampleInfo.volumeIndices[i] != IDX_NONE_32 && weightSum < 0.999f)
		{
			const DDGIVolumeGPUParams volumeParams = u_volumesDef.volumes[sampleInfo.volumeIndices[i]];
			const float3 luminanceSample = DDGISampleLuminanceInternal(sampleParams, context, volumeParams);
			if(all(luminanceSample > 0.0f))
			{
				const float weight = sampleInfo.weights[i];
				luminance += luminanceSample * weight;
				weightSum += weight;
			}
		}
	}

	return luminance / float(weightSum + 0.0001f);
}


class AverageLuminanceSampleCallback
{
	float3 Sample(in const DDGIVolumeGPUParams volumeParams, in uint3 probeWrappedCoords, in float2 octahedronUV)
	{
		float3 luminance = SampleProbeAverageLuminance(volumeParams, probeWrappedCoords);
		if(IsValid(luminance))
		{
			luminance = pow(luminance, volumeParams.probeIlluminanceEncodingGamma * 0.5f);
		}
		return luminance;
	}

	bool IsValid(in float3 sampleValue)
	{
		return all(sampleValue >= 0.0f);
	}

	float3 MakeInvalid()
	{
		return -1.f;
	}

	void PostProcess(inout float3 result)
	{
		result = Pow2(result);
	}
};


template<typename TSampleContext>
float3 DDGISampleAverageLuminanceInternal(in const DDGISampleParams sampleParams, in const TSampleContext context, in const DDGIVolumeGPUParams volumeParams)
{
	AverageLuminanceSampleCallback callback;
	return DDGISampleProbes<float3>(volumeParams, sampleParams, context, callback);
}


template<typename TSampleContext>
float3 DDGISampleAverageLuminance(in DDGISampleParams sampleParams, in const TSampleContext context)
{
	const DDGIVolumeSampleInfo sampleInfo = GetDDGIVolumeSampleInfo(sampleParams.worldLocation);

	for(uint i = 0; i < 2; ++i)
	{
		if (sampleInfo.volumeIndices[i] != IDX_NONE_32)
		{
			const DDGIVolumeGPUParams volumeParams = u_volumesDef.volumes[sampleInfo.volumeIndices[i]];
			const float3 luminance = DDGISampleAverageLuminanceInternal(sampleParams, context, volumeParams);
			if(all(luminance >= 0.0f))
			{
				return luminance;
			}
		}
	}

	return 0.f;
}


template<typename TSampleContext>
float3 DDGISampleIlluminanceBlended(in DDGISampleParams sampleParams, in const TSampleContext context)
{
	const float3 luminance = DDGISampleLuminanceBlended(sampleParams, context);

	return luminance * 2.f * PI; // multiply by integration domain area
}


template<typename TSampleContext>
float3 DDGISampleIlluminance(in DDGISampleParams sampleParams, in const TSampleContext context)
{
	const float3 luminance = DDGISampleLuminance(sampleParams, context);

	return luminance * 2.f * PI; // multiply by integration domain area
}
#endif // DS_DDGISceneDS

#endif // DDGI_TYPES_HLSLI
