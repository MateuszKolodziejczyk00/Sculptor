#include "SculptorShader.hlsli"

[[descriptor_set(SRATrousFilterDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/Wave.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"


struct CS_INPUT
{
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


float ComputeDetailPreservationStrength(in float historyLength, in float reprojectionConfidence)
{
	if(u_params.enableDetailPreservation)
	{
		const float historyLengthFactor = saturate((historyLength - 14.f) / 8.f);
		const float reprojectionConfidenceFactor = saturate((reprojectionConfidence - 0.9f) * 10.f);

		return historyLengthFactor * reprojectionConfidenceFactor;
	}
	else
	{
		return 0.f;
	}
}


float LoadVariance(in int2 coords)
{
	const uint2 clampedCoords = clamp(coords, int2(0, 0), int2(u_params.resolution - 1));
	return u_inputVarianceTexture.Load(uint3(clampedCoords, 0)).x;
}


float2 LoadVariance3x3(in uint2 groupOffset, in uint threadIdx, in uint2 localID)
{
	const uint2 pixel = groupOffset + localID;

	float upperLeft  = LoadVariance(pixel + int2(-1,  1));
	float upperRight = LoadVariance(pixel + int2( 1,  1));
	float lowerLeft  = LoadVariance(pixel + int2(-1, -1));
	float lowerRight = LoadVariance(pixel + int2( 1, -1));

	if(localID.x & 0x1)
	{
		Swap(lowerLeft, lowerRight);
		Swap(upperLeft, upperRight);
	}
	if(localID.y & 0x1)
	{
		Swap(lowerLeft, upperLeft);
		Swap(lowerRight, upperRight);
	}

	const uint4 horizontalSwizzle = uint4(1u, 0u, 3u, 2u);
	const uint4 verticalSwizzle   = uint4(2u, 3u, 0u, 1u);
	const uint4 diagonalSwizzle   = uint4(3u, 2u, 1u, 0u);

	const uint swizzleQuadBase = threadIdx & ~0x3;
	const uint quadLocalID     = threadIdx & 0x3;

	const float up   = QuadSwizzle(upperRight, swizzleQuadBase, quadLocalID, horizontalSwizzle);
	const float down = QuadSwizzle(lowerRight, swizzleQuadBase, quadLocalID, horizontalSwizzle);

	const float left  = QuadSwizzle(upperLeft, swizzleQuadBase, quadLocalID, verticalSwizzle);
	const float right = QuadSwizzle(upperRight, swizzleQuadBase, quadLocalID, verticalSwizzle);

	const float center = QuadSwizzle(upperRight, swizzleQuadBase, quadLocalID, diagonalSwizzle);

	const float variance3x3 = center * 0.25f
                            + (up + down + left + right) * 0.125f + 
                            + (upperRight + upperLeft + lowerLeft + lowerRight) * 0.0675f;

	return float2(center, variance3x3);
}


[numthreads(32, 1, 1)]
void SRATrousFilterCS(CS_INPUT input)
{
	const uint2 groupOffset = input.groupID.xy * uint2(8u, 4u);
	const uint threadIdx = input.localID.x;

	const uint2 localID = ReorderWaveToQuads(threadIdx);

	const int2 pixel = groupOffset + localID;
	
	if(all(pixel < u_params.resolution))
	{
		const float2 uv = (float2(pixel) + 0.5f) * u_params.invResolution;

		const float3 normal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));

		const float centerLinearDepth = u_linearDepthTexture.Load(uint3(pixel, 0));

		if(isinf(centerLinearDepth))
		{
			return;
		}

		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		if(roughness <= SPECULAR_TRACE_MAX_ROUGHNESS)
		{
			u_outputTexture[pixel] = u_inputTexture.Load(uint3(pixel, 0));
			return;
		}
		
		const float lumStdDevMultiplier = 1.5f;
		
		const float3 centerWS = LinearDepthToWS(u_sceneView, uv * 2.f - 1.f, centerLinearDepth);

		const float4 centerLuminanceHitDistance = u_inputTexture.Load(uint3(pixel, 0));
		const float lumCenter = Luminance(centerLuminanceHitDistance.rgb);

		const float accumulatedFrames = u_historyFramesNumTexture.Load(uint3(pixel, 0));
		const float specularReprojectionConfidence = u_reprojectionConfidenceTexture.Load(uint3(pixel, 0));

		float roughnessFilterStrength = 0.f;
		{
			roughnessFilterStrength = ComputeRoughnessFilterStrength(roughness, specularReprojectionConfidence, accumulatedFrames);
		}

		const float detailPreservationStrength = ComputeDetailPreservationStrength(accumulatedFrames, specularReprojectionConfidence);
		const float neighborWeight = lerp(1.f / 8.f, 1 / 32.f, detailPreservationStrength);

		const float kernel[2] = { 3.f / 8.f, neighborWeight };

		float weightSum = kernel[0];

		float3 luminanceSum = 0.f;
		luminanceSum += (centerLuminanceHitDistance.rgb / (Luminance(centerLuminanceHitDistance.rgb) + 1.f)) * weightSum;

		const float2 variance = LoadVariance3x3(groupOffset, threadIdx, localID);
		const float centerVariance = variance.y;

		const float centerLuminanceStdDev = centerVariance > 0.f ? sqrt(centerVariance) : 0.f;
		const float rcpLuminanceStdDev = 1.f / (centerLuminanceStdDev * lumStdDevMultiplier + 0.001f);

		float varianceSum = variance.x * Pow2(weightSum);

		const int radius = 1;

		[unroll]
		for (int y = -radius; y <= radius; ++y)
		{
			for (int x = -radius; x <= radius; ++x)
			{
				if (x == 0 && y == 0)
				{
					continue;
				}

				const int2 samplePixel = clamp(pixel + int2(x, y) * u_params.samplesOffset, int2(0, 0), int2(u_params.resolution - 1));
				float weight = 1.f;

				const float sampleLinearDepth = u_linearDepthTexture.Load(uint3(samplePixel, 0));
				const float sampleRoughness = u_roughnessTexture.Load(uint3(samplePixel, 0));
				const float3 sampleNormal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(samplePixel, 0)));
				float3 luminance = u_inputTexture.Load(uint3(samplePixel, 0)).rgb;

				if(isinf(sampleLinearDepth))
				{
					continue;
				}

				const float3 sampleWS = LinearDepthToWS(u_sceneView, samplePixel * u_params.invResolution * 2.f - 1.f, sampleLinearDepth);
				const float dw = ComputeWorldLocationWeight(centerWS, normal, sampleWS);
				weight *= dw;

				const float rw = ComputeRoughnessWeight(roughness, sampleRoughness, roughnessFilterStrength);
				weight *= rw;

				const float wn = ComputeSpecularNormalWeight(normal, sampleNormal, roughness);
				weight *= wn;

				weight *= kernel[max(abs(x), abs(y))];

				const float lum = Luminance(luminance);

				const float lw = (abs(lumCenter - lum) * rcpLuminanceStdDev + 0.001f);
				weight *= exp(-lw);

				luminance /= (lum + 1.f);

				luminanceSum += luminance * weight;

				const float sampleVariance = u_inputVarianceTexture.Load(uint3(samplePixel, 0)).x;

				varianceSum += sampleVariance * Pow2(weight);

				weightSum += weight;
			}
		}

		float3 outLuminance = luminanceSum / weightSum;
		outLuminance /= (1.f - Luminance(outLuminance));

		u_outputTexture[pixel] = float4(outLuminance, centerLuminanceHitDistance.w);

		const float outputVariance = varianceSum / Pow2(weightSum);
		u_outputVarianceTexture[pixel] = outputVariance;
	}
}
