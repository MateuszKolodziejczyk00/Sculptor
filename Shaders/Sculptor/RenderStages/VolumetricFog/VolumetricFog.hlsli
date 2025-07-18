#ifndef VOLUMETRIC_FOG_HLSLI
#define VOLUMETRIC_FOG_HLSLI

#include "Utils/SceneViewUtils.hlsli"

[[shader_struct(HeightFogParams)]]


float4 ComputeScatteringAndExtinction(in float3 fogAlbedo, in float fogExtinction, in float fogDensity)
{
	return float4(fogAlbedo * fogExtinction, fogExtinction) * fogDensity.xxxx;
}


static const float HEIGHT_FOG_MAX_HEIGHT = 330.f; // Maximum height for height-based fog, in meters


// based on https://advances.realtimerendering.com/s2006/Chapter6-Real-time%20Atmospheric%20Effects%20in%20Games.pdf
float EvaluateHeightBasedDensityAtLocation(float globalDensity, float3 location, float heightFalloff)
{
	if(location.z < HEIGHT_FOG_MAX_HEIGHT)
	{
		return globalDensity * exp(-heightFalloff * location.z);
	}

	return 0.f;
}


float3 ClipVolumetricFogSegmentEndLocation(in float3 start, inout float3 end, inout float3 segment, inout float segmentLength)
{
	if(end.z > HEIGHT_FOG_MAX_HEIGHT)
	{
		const float t = (HEIGHT_FOG_MAX_HEIGHT - start.z) / segment.z;

		end           = start + segment * t;
		segmentLength = segmentLength * t;
		segment       = end - start;
	}

	return end;
}


// This function currently absorbs too much energy. It assumes that fog only absorbs lighting, without any scattering, which is clearly wrong but I guess better than nothing :)
float EvaluateHeightBasedTransmittanceForSegment(in HeightFogParams fogParams, in float3 start, in float3 end)
{
	float3 segment = end - start;
	float segmentLenth = length(segment);

	if(start.z > end.z)
	{
		Swap(start, end);
	}

	if(start.z > HEIGHT_FOG_MAX_HEIGHT)
	{
		return 1.f; // full transmittance if both points are above the maximum height
	}

	ClipVolumetricFogSegmentEndLocation(start, end, segment, segmentLenth);

	const float z0 = start.z;
	const float z1 = end.z;
	const float dz = z1 - z0;
	
	const float extinction = fogParams.extinction;
	const float density    = fogParams.density;
	const float falloff    = fogParams.heightFalloff;

	float opticalDepth;

	if(dz > 1e-5f)
	{
		// Integral of expt(-falloff * z) is:
		// -1 / falloff * exp(-falloff * z) + C

		// Definite integral of exp(-falloff * z) from z0 to z1
		// = (-1 / falloff) * exp(-falloff * z1) - (-1 / falloff) * exp(-falloff * z0)
		// = (1 / falloff) * (exp(-falloff * z0) - exp(-falloff * z1))

		// We also divide by dz to get the average density over along vertical segment
		// which will be then multiplied by the segment length to get final optical depth

		const float integral = (exp(-max(falloff * z0, 1e-6f)) - exp(-max(falloff * z1, 1e-6f))) * rcp(falloff * dz);

		opticalDepth = integral * density * extinction * segmentLenth;

	}
	else
	{
		opticalDepth = exp(-falloff * z0) * density * extinction * segmentLenth;
	}

	const float transmittance = exp(-max(opticalDepth, 1e-6f));
	return transmittance;
}


float ComputeFogFroxelLinearDepth(in float depth, in float fogNearPlane, in float fogFarPlane)
{
	return fogNearPlane * pow(fogFarPlane / fogNearPlane, depth);
}


float3 FogFroxelToNDC(in float2 fogFroxelUV, in float fogFroxelLinearDepth, in float projectionNear)
{
	return float3(fogFroxelUV * 2.f - 1.f, projectionNear / fogFroxelLinearDepth);
}


float3 ComputeFogFroxelUVW(in float2 uv, in float linearDepth, in float fogNearPlane, in float fogFarPlane)
{
	const float w = log(linearDepth / fogNearPlane) / log(fogFarPlane / fogNearPlane);
	return float3(uv, w);
}

#ifdef DS_RenderVolumetricFogDS

float2 ComputeFogFroxelDepthBounds(in VolumetricFogConstants fogConstants, in uint froxelZ, float3 rcpResolution)
{
	const float depthBegin = ComputeFogFroxelLinearDepth(froxelZ * rcpResolution.z, fogConstants.fogNearPlane, fogConstants.fogFarPlane);
	const float depthEnd = ComputeFogFroxelLinearDepth((froxelZ + 1) * rcpResolution.z, fogConstants.fogNearPlane, fogConstants.fogFarPlane);
	return float2(depthBegin, depthEnd);
}

float ComputeFogDepthJitter(in VolumetricFogConstants fogConstants, in uint2 pixel)
{
	const uint2 blueNoiseTexturePixel = pixel & fogConstants.blueNoiseResMask;
	const float blueNoise = u_blueNoiseTexture.Load(uint3(blueNoiseTexturePixel, 0)).x;
	const float goldenRationSequence = (fogConstants.frameIdx & 31u) * SPT_GOLDEN_RATIO;
	return (frac(blueNoise + goldenRationSequence) - 0.5f);
}

float3 ComputeFogGridSampleUVW(in VolumetricFogConstants fogConstants, in SceneViewData sceneView, in uint3 froxel, in uint3 resolution, in Texture2D<float> depthTexture, in SamplerState depthSampler, float bias = 0.05f)
{
	const float3 rcpResolution = 1.f / float3(resolution);
	const float2 froxelLinearDepthBounds = ComputeFogFroxelDepthBounds(fogConstants, froxel.z, rcpResolution);

	const float depthJitter = ComputeFogDepthJitter(fogConstants, froxel.xy);

	// jitter 2d is premultiplied by inv resolution
	const float3 jitter = float3(fogConstants.jitter2D, depthJitter * rcpResolution.z);

	float3 fogFroxelUVW = (float3(froxel) + 0.5f) * rcpResolution + jitter;

	const float geometryDepth = depthTexture.SampleLevel(depthSampler, fogFroxelUVW.xy, 0).r;
	const float geometryLinearDepth = geometryDepth > 0.0f ? ComputeLinearDepth(geometryDepth, sceneView) : -1.f;
	bias = min(bias, geometryLinearDepth * 0.2f);

	if(geometryLinearDepth + bias > froxelLinearDepthBounds.x && geometryLinearDepth < froxelLinearDepthBounds.y)
	{
		fogFroxelUVW.z -= rcpResolution.z;
	}

	return fogFroxelUVW;
}

#endif // DS_RenderVolumetricFogDS

#endif // VOLUMETRIC_FOG_HLSLI
