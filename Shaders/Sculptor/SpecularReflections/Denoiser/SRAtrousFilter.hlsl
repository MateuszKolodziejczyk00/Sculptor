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


float LoadVariance(in Texture2D<float> varianceTexture, in int2 coords)
{
	const uint2 clampedCoords = clamp(coords, int2(0, 0), int2(u_params.resolution - 1));
	return varianceTexture.Load(uint3(clampedCoords, 0)).x;
}


float2 LoadVariance3x3(in Texture2D<float> varianceTexture, in uint2 groupOffset, in uint threadIdx, in uint2 localID)
{
	const uint2 pixel = groupOffset + localID;

	float upperLeft  = LoadVariance(varianceTexture, pixel + int2(-1,  1));
	float upperRight = LoadVariance(varianceTexture, pixel + int2( 1,  1));
	float lowerLeft  = LoadVariance(varianceTexture, pixel + int2(-1, -1));
	float lowerRight = LoadVariance(varianceTexture, pixel + int2( 1, -1));

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
			u_outDiffuseLuminance[pixel] = u_inDiffuseLuminance.Load(uint3(pixel, 0));
			u_outSpecularLuminance[pixel] = u_inSpecularLuminance.Load(uint3(pixel, 0));
			return;
		}
		
		const float lumStdDevMultiplier = 1.5f;
		
		const float3 centerWS = LinearDepthToWS(u_sceneView, uv * 2.f - 1.f, centerLinearDepth);

		const float accumulatedFrames = u_historyFramesNumTexture.Load(uint3(pixel, 0));
		const float specularReprojectionConfidence = u_reprojectionConfidenceTexture.Load(uint3(pixel, 0));

		const float roughnessFilterStrength = ComputeRoughnessFilterStrength(roughness, specularReprojectionConfidence, accumulatedFrames);

		const float detailPreservationStrength = ComputeDetailPreservationStrength(accumulatedFrames, specularReprojectionConfidence);
		const float neighborWeight = lerp(1.f / 8.f, 1 / 32.f, detailPreservationStrength);

		const float4 centerSpecular = u_inSpecularLuminance.Load(uint3(pixel, 0));
		const float specularLumCenter = Luminance(centerSpecular.rgb);

		const float4 centerDiffuse = u_inDiffuseLuminance.Load(uint3(pixel, 0));
		const float diffuseLumCenter = Luminance(centerDiffuse.rgb);

		const float kernel[2] = { 3.f / 8.f, neighborWeight };

		float specularWeightSum = kernel[0];
		float diffuseWeightSum = kernel[0];

		float3 specularSum = 0.f;
		specularSum += (centerSpecular.rgb / (Luminance(centerSpecular.rgb) + 1.f)) * specularWeightSum;

		float3 diffuseSum = 0.f;
		diffuseSum += (centerDiffuse.rgb / (Luminance(centerDiffuse.rgb) + 1.f)) * diffuseWeightSum;

		const float2 specularVariance = LoadVariance3x3(u_inSpecularVariance, groupOffset, threadIdx, localID);
		const float2 diffuseVariance  = LoadVariance3x3(u_inDiffuseVariance, groupOffset, threadIdx, localID);
		const float centerSpecularVariance = specularVariance.y;
		const float centerDiffuseVariance  = diffuseVariance.y;

		const float centerSpecularLumStdDev = centerSpecularVariance > 0.f ? sqrt(centerSpecularVariance) : 0.f;
		const float rcpSpecularLumStdDev = 1.f / (centerSpecularLumStdDev * lumStdDevMultiplier + 0.001f);
		float specularVarianceSum = specularVariance.x * Pow2(specularWeightSum);

		const float centerDiffuseLumStdDev = centerDiffuseVariance > 0.f ? sqrt(centerDiffuseVariance) : 0.f;
		const float rcpDiffuseLumStdDev = 1.f / (centerDiffuseLumStdDev * lumStdDevMultiplier + 0.001f);
		float diffuseVarianceSum = diffuseVariance.x * Pow2(diffuseWeightSum);

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

				float3 specular = u_inSpecularLuminance.Load(uint3(samplePixel, 0)).rgb;
				float3 diffuse  = u_inDiffuseLuminance.Load(uint3(samplePixel, 0)).rgb;

				if(isinf(sampleLinearDepth))
				{
					continue;
				}

				const float3 sampleWS = LinearDepthToWS(u_sceneView, samplePixel * u_params.invResolution * 2.f - 1.f, sampleLinearDepth);
				const float dw = ComputeWorldLocationWeight(centerWS, normal, sampleWS);
				weight *= dw;

				weight *= kernel[max(abs(x), abs(y))];

				const float specularLum = Luminance(specular);

				float specularWeight = weight;

				const float rw = ComputeRoughnessWeight(roughness, sampleRoughness, roughnessFilterStrength);
				specularWeight *= rw;

				const float slw = (abs(specularLumCenter - specularLum) * rcpSpecularLumStdDev + 0.001f);
				specularWeight *= exp(-slw);

				const float swn = ComputeSpecularNormalWeight(normal, sampleNormal, roughness);
				specularWeight *= swn;

				specular /= (specularLum + 1.f);

				specularSum += specular * specularWeight;

				const float sampleSpecularVariance = u_inSpecularVariance.Load(uint3(samplePixel, 0)).x;

				specularVarianceSum += sampleSpecularVariance * Pow2(specularWeight);

				specularWeightSum += specularWeight;

				float diffuseWeight = weight;

				const float diffuseLum = Luminance(diffuse);

				const float dlw = (abs(diffuseLumCenter - diffuseLum) * rcpDiffuseLumStdDev + 0.001f);
				diffuseWeight *= exp(-dlw);

				const float dwn = ComputeDiffuseNormalWeight(normal, sampleNormal);
				diffuseWeight *= dwn;

				diffuse /= (diffuseLum + 1.f);

				diffuseSum += diffuse * diffuseWeight;

				const float sampleDiffuseVariance = u_inDiffuseVariance.Load(uint3(samplePixel, 0u)).x;

				diffuseVarianceSum += sampleDiffuseVariance * Pow2(diffuseWeight);

				diffuseWeightSum += diffuseWeight;
			}
		}

		float3 outSpecular = specularSum / specularWeightSum;
		outSpecular /= (1.f - Luminance(outSpecular ));
		u_outSpecularLuminance[pixel] = float4(outSpecular, centerSpecular.w);

		float3 outDiffuse = diffuseSum / diffuseWeightSum;
		outDiffuse /= (1.f - Luminance(outDiffuse));
		u_outDiffuseLuminance[pixel] = float4(outDiffuse, centerDiffuse.w);

		const float outSpecularVariance = specularVarianceSum / Pow2(specularWeightSum);
		u_outSpecularVariance[pixel] = outSpecularVariance;

		const float outDiffuseVariance = diffuseVarianceSum / Pow2(diffuseWeightSum);
		u_outDiffuseVariance[pixel] = outDiffuseVariance;
	}
}
