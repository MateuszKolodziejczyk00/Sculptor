#include "SculptorShader.hlsli"

[[descriptor_set(SRATrousFilterDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/Wave.hlsli"
#include "SpecularReflections/RTGICommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"
#include "SpecularReflections/Denoiser/RTDenoising.hlsli"


struct CS_INPUT
{
	uint3 groupID : SV_GroupID;
	uint3 localID : SV_GroupThreadID;
};


float2 LoadVariance(in Texture2D<float2> varianceTexture, in int2 coords)
{
	const uint2 clampedCoords = clamp(coords, int2(0, 0), int2(u_constants.resolution - 1));
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
	
	if(all(pixel < u_constants.resolution))
	{
		const float2 uv = (float2(pixel) + 0.5f) * u_constants.invResolution;

		const float3 normal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));

		const float centerLinearDepth = u_linearDepthTexture.Load(uint3(pixel, 0));

		if(isinf(centerLinearDepth))
		{
			return;
		}

		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		if(roughness <= SPECULAR_TRACE_MAX_ROUGHNESS)
		{
			const RTSphericalBasis outSpecularY_SH = RawToRTSphericalBasis(u_constants.inSpecularY.Load(uint3(pixel, 0)));
			const RTSphericalBasis outDiffuseY_SH = RawToRTSphericalBasis(u_constants.inDiffuseY.Load(uint3(pixel, 0)));
			const float4 outDiffSpecCoCg = u_constants.inDiffSpecCoCg.Load(uint3(pixel, 0));
#if OUTPUT_SH
			u_constants.rwSpecularY.Store(pixel, RTSphericalBasisToRaw(outSpecularY_SH));
			u_constants.rwDiffuseY.Store(pixel, RTSphericalBasisToRaw(outDiffuseY_SH));
			u_constants.rwDiffSpecCoCg.Store(pixel, outDiffSpecCoCg);
#else
			const float3 outSpecularYCoCg = float3(outSpecularY_SH.Evaluate(normal), outDiffSpecCoCg.zw);
			const float3 outDiffuseYCoCg  = float3(outDiffuseY_SH.Evaluate(normal), outDiffSpecCoCg.xy);

			const float3 outSpecular = YCoCgToRGB(outSpecularYCoCg);
			const float3 outDiffuse  = YCoCgToRGB(outDiffuseYCoCg);

			u_constants.rwSpecularRGB.Store(pixel, outSpecular);
			u_constants.rwDiffuseRGB.Store(pixel, outDiffuse);
#endif // OUTPUT_SH
			return;
		}
		
		const float lumStdDevMultiplier = 1.2f;
		
		const float3 centerWS = LinearDepthToWS(u_sceneView, uv * 2.f - 1.f, centerLinearDepth);

		const float specularHistoryLength = u_specularHistoryLengthTexture.Load(uint3(pixel, 0));
		const float roughnessFilterStrength = ComputeRoughnessFilterStrength(roughness, specularHistoryLength);

		const RTSphericalBasis specularY_SH = RawToRTSphericalBasis(u_constants.inSpecularY.Load(uint3(pixel, 0)));
		const RTSphericalBasis diffuseY_SH  = RawToRTSphericalBasis(u_constants.inDiffuseY.Load(uint3(pixel, 0)));

		const float specularLumCenter = specularY_SH.Evaluate(normal);
		const float diffuseLumCenter  = diffuseY_SH.Evaluate(normal);

#if WIDE_RADIUS
		const float kernel[3] = { 3.f / 8.f, 2.f / 8.f, 1.f / 8.f };
#else
		const float kernel[2] = { 3.f / 8.f, 1.f / 8.f };
#endif // WIDE_RADIUS

		float specularWeightSum = kernel[0];
		float diffuseWeightSum  = kernel[0];

		RTSphericalBasis specularY_SH_Sum = specularY_SH * kernel[0];
		RTSphericalBasis diffuseY_SH_Sum  = diffuseY_SH * kernel[0];
		float4 diffSpecCoCg_Sum     = u_constants.inDiffSpecCoCg.Load(uint3(pixel, 0)) * kernel[0];

		const ATrousVarianceData varianceData = LoadVariance3x3(u_inVariance, groupOffset, localID);

		const float centerSpecularVariance = varianceData.center.x;
		const float centerDiffuseVariance  = varianceData.center.y;

		const float centerSpecularLumStdDev = centerSpecularVariance > 0.f ? sqrt(centerSpecularVariance) : 0.f;
		const float rcpSpecularLumStdDev = 1.f / (centerSpecularLumStdDev * lumStdDevMultiplier + 0.001f);
		float specularVarianceSum = varianceData.neighboorhood.x * Pow2(specularWeightSum);

		const float centerDiffuseLumStdDev = centerDiffuseVariance > 0.f ? sqrt(centerDiffuseVariance) : 0.f;
		const float rcpDiffuseLumStdDev = 1.f / (centerDiffuseLumStdDev * lumStdDevMultiplier + 0.001f);
		float diffuseVarianceSum = varianceData.neighboorhood.y * Pow2(diffuseWeightSum);


#if WIDE_RADIUS
		const int radius = 2;
#else
		const int radius = 1;
#endif // WIDE_RADIUS

		for (int y = -radius; y <= radius; ++y)
		{
			[unroll]
			for (int x = -radius; x <= radius; ++x)
			{
				if (x == 0 && y == 0)
				{
					continue;
				}

				const int2 samplePixel = clamp(pixel + int2(x, y) * u_constants.samplesOffset, int2(0, 0), int2(u_constants.resolution - 1));
				float weight = 1.f;

				const float sampleLinearDepth = u_linearDepthTexture.Load(uint3(samplePixel, 0));
				const float sampleRoughness = u_roughnessTexture.Load(uint3(samplePixel, 0));
				const float3 sampleNormal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(samplePixel, 0)));
				SPT_CHECK_MSG(all(!isnan(sampleNormal)) && all(!isinf(sampleNormal)), L"Invalid sample normal");

				RTSphericalBasis sampleSpecularY_SH = RawToRTSphericalBasis(u_constants.inSpecularY.Load(uint3(samplePixel, 0)));
				RTSphericalBasis sampleDiffuseY_SH  = RawToRTSphericalBasis(u_constants.inDiffuseY.Load(uint3(samplePixel, 0)));
				const float4 sampleDiffSpecCoCg     = u_constants.inDiffSpecCoCg.Load(uint3(samplePixel, 0));

				if(isinf(sampleLinearDepth))
				{
					continue;
				}

				const float3 sampleWS = LinearDepthToWS(u_sceneView, samplePixel * u_constants.invResolution * 2.f - 1.f, sampleLinearDepth);
				const float dw = ComputeWorldLocationWeight(centerWS, normal, sampleWS);
				weight *= dw;

				weight *= kernel[max(abs(x), abs(y))];

				const float specularLum = sampleSpecularY_SH.Evaluate(sampleNormal);

				float specularWeight = weight;

				const float rw = ComputeRoughnessWeight(roughness, sampleRoughness, roughnessFilterStrength);
				specularWeight *= rw;

				const float slw = (abs(specularLumCenter - specularLum) * rcpSpecularLumStdDev + 0.001f);
				specularWeight *= exp(-slw);

				const float swn = ComputeSpecularNormalWeight(normal, sampleNormal, roughness);
				specularWeight *= swn;

				specularY_SH_Sum = specularY_SH_Sum + (sampleSpecularY_SH * specularWeight);

				const float2 variance = u_inVariance.Load(uint3(samplePixel, 0));
				const float sampleSpecularVariance = variance.x;

				specularVarianceSum += sampleSpecularVariance * Pow2(specularWeight);

				specularWeightSum += specularWeight;

				float diffuseWeight = weight;

				const float diffuseLum = sampleDiffuseY_SH.Evaluate(sampleNormal);

				const float dlw = (abs(diffuseLumCenter - diffuseLum) * rcpDiffuseLumStdDev + 0.001f);
				diffuseWeight *= exp(-dlw);

				const float dwn = ComputeDiffuseNormalWeight(normal, sampleNormal);
				diffuseWeight *= dwn;

				diffuseY_SH_Sum = diffuseY_SH_Sum + (sampleDiffuseY_SH * diffuseWeight);

				const float sampleDiffuseVariance = variance.y;

				diffuseVarianceSum += sampleDiffuseVariance * Pow2(diffuseWeight);

				diffuseWeightSum += diffuseWeight;

				diffSpecCoCg_Sum += sampleDiffSpecCoCg * float4(diffuseWeight, diffuseWeight, specularWeight, specularWeight);
			}
		}

		RTSphericalBasis outSpecularY_SH = specularY_SH_Sum / specularWeightSum;

		RTSphericalBasis outDiffuseY_SH = diffuseY_SH_Sum / diffuseWeightSum;

		const float4 outDiffSpecCoCg = diffSpecCoCg_Sum / float4(diffuseWeightSum, diffuseWeightSum, specularWeightSum, specularWeightSum);

#if OUTPUT_SH
		u_constants.rwSpecularY.Store(pixel, RTSphericalBasisToRaw(outSpecularY_SH));
		u_constants.rwDiffuseY.Store(pixel, RTSphericalBasisToRaw(outDiffuseY_SH));
		u_constants.rwDiffSpecCoCg.Store(pixel, outDiffSpecCoCg);

		const float outSpecularVariance = specularVarianceSum / Pow2(specularWeightSum);
		const float outDiffuseVariance = diffuseWeightSum > 0.f ? diffuseVarianceSum / Pow2(diffuseWeightSum) : 0.f;

		u_outVariance[pixel] = float2(outSpecularVariance, outDiffuseVariance);
#else
		const float3 outSpecularYCoCg = float3(outSpecularY_SH.Evaluate(normal), outDiffSpecCoCg.zw);
		const float3 outDiffuseYCoCg  = float3(outDiffuseY_SH.Evaluate(normal), outDiffSpecCoCg.xy);

		const float3 outSpecular = YCoCgToRGB(outSpecularYCoCg);
		const float3 outDiffuse  = YCoCgToRGB(outDiffuseYCoCg);

		u_constants.rwSpecularRGB.Store(pixel, outSpecular);
		u_constants.rwDiffuseRGB.Store(pixel, outDiffuse);
#endif
	}
}
