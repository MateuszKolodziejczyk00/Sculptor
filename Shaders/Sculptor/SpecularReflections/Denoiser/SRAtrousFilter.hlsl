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


float2 LoadVariance(in Texture2D<float2> varianceTexture, in int2 coords)
{
	const uint2 clampedCoords = clamp(coords, int2(0, 0), int2(u_params.resolution - 1));
	return varianceTexture.Load(uint3(clampedCoords, 0));
}

#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8

#define GROUP_CACHE_SIZE_X (GROUP_SIZE_X + 2)
#define GROUP_CACHE_SIZE_Y (GROUP_SIZE_Y + 2)


groupshared float2 gs_cachedVardiance[GROUP_CACHE_SIZE_Y][GROUP_CACHE_SIZE_X];


void CacheGroupVariance(in Texture2D<float2> varianceTexture, in uint2 groupOffset, in uint2 localID)
{
	const int2 varianceLoadOffset = int2(groupOffset) - 1;

	const uint threadIdx = localID.y * GROUP_SIZE_X + localID.x;

	const uint samplesNum = (GROUP_CACHE_SIZE_X * GROUP_CACHE_SIZE_Y);
	const uint groupSize = (GROUP_SIZE_X * GROUP_SIZE_Y);

	for (uint sampleIdx = threadIdx; sampleIdx < samplesNum; sampleIdx += groupSize)
	{
		const int localY = sampleIdx / GROUP_CACHE_SIZE_Y;
		const int localX = sampleIdx - (localY * GROUP_CACHE_SIZE_Y);
		const int2 sampleCoords = varianceLoadOffset + int2(localX, localY);

		float2 variance = LoadVariance(varianceTexture, sampleCoords);
		if(isnan(variance.x) || isinf(variance.x))
		{
			variance.x = 0.f;
		}
		if(isnan(variance.y) || isinf(variance.y))
		{
			variance.y = 0.f;
		}
		gs_cachedVardiance[localY][localX] = variance;
	}
}


struct ATrousVarianceData
{
	float2 center;
	float2 neighboorhood;
};


ATrousVarianceData LoadVariance3x3(in Texture2D<float2> varianceTexture, in uint2 groupOffset, in uint2 localID)
{
	GroupMemoryBarrierWithGroupSync();

	const float2 center = gs_cachedVardiance[localID.y + 1][localID.x + 1];

	const float2 up    = gs_cachedVardiance[localID.y][localID.x + 1];
	const float2 down  = gs_cachedVardiance[localID.y + 2][localID.x + 1];
	const float2 left  = gs_cachedVardiance[localID.y + 1][localID.x];
	const float2 right = gs_cachedVardiance[localID.y + 1][localID.x + 2];
	
	const float2 upperLeft  = gs_cachedVardiance[localID.y][localID.x];
	const float2 upperRight = gs_cachedVardiance[localID.y][localID.x + 2];
	const float2 lowerLeft  = gs_cachedVardiance[localID.y + 2][localID.x];
	const float2 lowerRight = gs_cachedVardiance[localID.y + 2][localID.x + 2];

	const float2 variance3x3 = center * 0.25f
                            + (up + down + left + right) * 0.125f + 
                            + (upperRight + upperLeft + lowerLeft + lowerRight) * 0.0675f;

	ATrousVarianceData varianceData;
	varianceData.center        = center;
	varianceData.neighboorhood = variance3x3;

	return varianceData;
}


[numthreads(8, 8, 1)]
void SRATrousFilterCS(CS_INPUT input)
{
	const uint2 groupOffset = input.groupID.xy * uint2(GROUP_SIZE_X, GROUP_SIZE_Y);

	const uint2 localID = input.localID.xy;

	const int2 pixel = groupOffset + localID;

	CacheGroupVariance(u_inVariance, groupOffset, localID);
	
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
			u_outVariance[pixel] = 0.f;
			return;
		}
		
		const float lumStdDevMultiplier = 1.2f;
		
		const float3 centerWS = LinearDepthToWS(u_sceneView, uv * 2.f - 1.f, centerLinearDepth);

		const float specularHistoryLength = u_specularHistoryLengthTexture.Load(uint3(pixel, 0));
		const float roughnessFilterStrength = ComputeRoughnessFilterStrength(roughness, specularHistoryLength);

		const float4 centerSpecular = u_inSpecularLuminance.Load(uint3(pixel, 0));
		const float specularLumCenter = Luminance(centerSpecular.rgb);

		const float4 centerDiffuse = u_inDiffuseLuminance.Load(uint3(pixel, 0));
		const float diffuseLumCenter = Luminance(centerDiffuse.rgb);

		const float kernel[2] = { 3.f / 8.f, 1.f / 8.f };

		float specularWeightSum = kernel[0];
		float diffuseWeightSum = kernel[0];

		float3 specularSum = 0.f;
		specularSum += (centerSpecular.rgb / (Luminance(centerSpecular.rgb) + 1.f)) * specularWeightSum;

		float3 diffuseSum = 0.f;
		diffuseSum += (centerDiffuse.rgb / (Luminance(centerDiffuse.rgb) + 1.f)) * diffuseWeightSum;

		const ATrousVarianceData varianceData = LoadVariance3x3(u_inVariance, groupOffset, localID);

		const float centerSpecularVariance = varianceData.center.x;
		const float centerDiffuseVariance  = varianceData.center.y;

		const float centerSpecularLumStdDev = centerSpecularVariance > 0.f ? sqrt(centerSpecularVariance) : 0.f;
		const float rcpSpecularLumStdDev = 1.f / (centerSpecularLumStdDev * lumStdDevMultiplier + 0.001f);
		float specularVarianceSum = varianceData.neighboorhood.x * Pow2(specularWeightSum);

		const float centerDiffuseLumStdDev = centerDiffuseVariance > 0.f ? sqrt(centerDiffuseVariance) : 0.f;
		const float rcpDiffuseLumStdDev = 1.f / (centerDiffuseLumStdDev * lumStdDevMultiplier + 0.001f);
		float diffuseVarianceSum = varianceData.neighboorhood.y * Pow2(diffuseWeightSum);

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
				SPT_CHECK_MSG(all(!isnan(sampleNormal)) && all(!isinf(sampleNormal)), L"Invalid sample normal");

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

				const float2 variance = u_inVariance.Load(uint3(samplePixel, 0));
				const float sampleSpecularVariance = variance.x;

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

				const float sampleDiffuseVariance = variance.y;

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
		const float outDiffuseVariance = diffuseWeightSum > 0.f ? diffuseVarianceSum / Pow2(diffuseWeightSum) : 0.f;

		u_outVariance[pixel] = float2(outSpecularVariance, outDiffuseVariance);
	}
}
