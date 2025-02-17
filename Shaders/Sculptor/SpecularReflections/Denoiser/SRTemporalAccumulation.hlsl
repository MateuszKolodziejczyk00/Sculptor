#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(SRTemporalAccumulationDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
    uint3 groupID : SV_GroupID;
    uint3 localID : SV_GroupThreadID;
};


#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8


#define HISTORY_SAMPLE_COUNT 5

static const int2 g_historyOffsets[HISTORY_SAMPLE_COUNT] =
{
	int2(0, 0),
	int2(-1, 0),
	int2(1, 0),
	int2(0, 1),
	int2(0, -1)
};

void FindTemporalReprojection(in float2 surfaceHistoryUV, 
                              in float3 currentSampleNormal,
                              in float3 currentSampleWS,
                              in float2 pixelSize,
                              in float maxPlaneDistance,
                              in float roughness,
                              out float3 reprojectedWS,
                              out float2 reprojectedUV,
                              out float reprojectionConfidence)
{
	reprojectedUV = surfaceHistoryUV;

	const float maxRoguhnessDiff = roughness * 0.1f;

	reprojectionConfidence = 0.f;

	const Plane currentSamplePlane = Plane::Create(currentSampleNormal, currentSampleWS);

	if (all(reprojectedUV >= 0.f) && all(reprojectedUV <= 1.f))
	{
		const float historySampleDepth = u_historyDepthTexture.SampleLevel(u_nearestSampler, reprojectedUV, 0.f);

		for(uint sampleIdx = 0; sampleIdx < HISTORY_SAMPLE_COUNT; ++sampleIdx)
		{
			const float2 sampleUV = reprojectedUV + g_historyOffsets[sampleIdx] * pixelSize;

			const float3 historySampleNDC = float3(sampleUV * 2.f - 1.f, historySampleDepth);
			const float3 historySampleWS = NDCToWorldSpaceNoJitter(historySampleNDC, u_prevFrameSceneView);

			const float historySampleDistToCurrentSamplePlane = currentSamplePlane.Distance(historySampleWS);
			if (abs(historySampleDistToCurrentSamplePlane) < maxPlaneDistance)
			{
				const float3 historyNormals  = OctahedronDecodeNormal(u_historyNormalsTexture.SampleLevel(u_nearestSampler, sampleUV, 0.f));
				const float historyRoughness = u_historyRoughnessTexture.SampleLevel(u_nearestSampler, sampleUV, 0.f);

				const float normalsSimilarity   = dot(historyNormals, currentSampleNormal);
				const float roughnessDifference = abs(roughness - historyRoughness);

				if(normalsSimilarity >= 0.9f && roughnessDifference < maxRoguhnessDiff)
				{
					const bool isCenterSample = sampleIdx == 0;

					const float sampleReprojectionConfidence = Pow3(normalsSimilarity) * (1.f - roughnessDifference);

					if(sampleReprojectionConfidence > reprojectionConfidence)
					{
						reprojectionConfidence = sampleReprojectionConfidence;
						reprojectedUV = sampleUV;
						reprojectedWS = historySampleWS;

						if(reprojectionConfidence > 0.99f)
						{
							break;
						}
					}
				}
			}
		}
	}
}


struct NeighbourhoodInfo
{
	float4 specularM1;
	float4 specularM2;

	float minHitDist;
};

#define GS_RADIUS 3

#define GS_DATA_SIZE_Y (GROUP_SIZE_Y + 2 * GS_RADIUS)
#define GS_DATA_SIZE_X (GROUP_SIZE_X + 2 * GS_RADIUS)


groupshared float4 gs_specular[GS_DATA_SIZE_Y][GS_DATA_SIZE_X];


void PrecacheLocalSpecularValues(in int2 groupID, in int2 localID, in int2 resolution)
{
	const int2 groupOffset = groupID * int2(GROUP_SIZE_X, GROUP_SIZE_Y);

	const uint samplesNum = GS_DATA_SIZE_X * GS_DATA_SIZE_Y;

	const uint groupThreadID = localID.x + (localID.y * GROUP_SIZE_X);

	const int2 maxCoords = resolution - 1;

	for (uint sampleIdx = groupThreadID; sampleIdx < samplesNum; sampleIdx += (GROUP_SIZE_X * GROUP_SIZE_Y))
	{
		const int sampleY = sampleIdx / GS_DATA_SIZE_X;
		const int sampleX = sampleIdx - sampleY * GS_DATA_SIZE_X;
		const int2 sampleCoords = clamp(groupOffset + int2(sampleX, sampleY) - GS_RADIUS, 0, maxCoords);

		gs_specular[sampleY][sampleX] = u_rwSpecularTexture[sampleCoords];
	}
}


NeighbourhoodInfo LoadNeighbourhoodInfo(in int2 localID, in int2 resolution)
{
	float4 specularM1 = 0.f;
	float4 specularM2 = 0.f;
	float minHitDist = 9999999999.f;

	float validSamplesNum = 0.f;

    GroupMemoryBarrierWithGroupSync();

	for(int y = -GS_RADIUS; y <= GS_RADIUS; ++y)
	{
		for(int x = -GS_RADIUS; x <= GS_RADIUS; ++x)
		{
			const float4 sample = gs_specular[localID.y + GS_RADIUS + y][localID.x + GS_RADIUS + x];

			if(!isnan(sample.w))
			{
				specularM1 += sample;
				specularM2 += Pow2(sample);

				minHitDist = min(minHitDist, sample.w);

				validSamplesNum += 1.f;
			}
		}
	}

	const float rcpSamplesNum = validSamplesNum > 0.f ? rcp(validSamplesNum) : 0.f;

	specularM1 *= rcpSamplesNum;
	specularM2 *= rcpSamplesNum;

	NeighbourhoodInfo result;
	result.specularM1 = specularM1;
	result.specularM2 = specularM2;
	result.minHitDist = minHitDist;

	return result;
}

#define USE_MIN_HIT_DIST 1
float ComputeHitDistance(in NeighbourhoodInfo info)
{
#if USE_MIN_HIT_DIST
	return info.minHitDist;
#else
	const float hitDistVariance = abs(Pow2(info.specularM1.w) - info.specularM2.w);
	const float hitDistStdDev = sqrt(hitDistVariance);
	const float hitDistClampWindow = hitDistStdDev * 3.f;
	return clamp(info.minHitDist, info.specularM1.w - hitDistClampWindow, info.specularM1.w + hitDistClampWindow);
#endif // USE_MIN_HIT_DIST
}

#define TEST 1

// Algorithm based on RELAX temporal accumulation: https://github.com/NVIDIAGameWorks/RayTracingDenoiser/blob/master/Shaders/Include/RELAX_TemporalAccumulation.hlsli
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void SRTemporalAccumulationCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	
	uint2 outputRes;
	u_rwSpecularTexture.GetDimensions(outputRes.x, outputRes.y);

	PrecacheLocalSpecularValues(int2(input.groupID.xy), int2(input.localID.xy), int2(outputRes));

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		const float2 pixelSize = rcp(float2(outputRes));
		const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

		const float4 specular = u_rwSpecularTexture[pixel];
		const float4 diffuse  = u_rwDiffuseTexture[pixel];

		const float currentDepth = u_depthTexture.Load(uint3(pixel, 0));
		if(currentDepth == 0.f)
		{
			return;
		}

		const NeighbourhoodInfo neighbourhoodInfo = LoadNeighbourhoodInfo(int2(input.localID.xy), int2(outputRes));

		const float hitDistance = ComputeHitDistance(neighbourhoodInfo);

		const float3 currentNDC = float3(uv * 2.f - 1.f, currentDepth);
		const float3 currentSampleWS = NDCToWorldSpaceNoJitter(currentNDC, u_sceneView);
		const float3 projectedReflectionWS = currentSampleWS + normalize(currentSampleWS - u_sceneView.viewLocation) * neighbourhoodInfo.specularM1.w;
		const float3 prevFrameNDC = WorldSpaceToNDCNoJitter(projectedReflectionWS, u_prevFrameSceneView);

		const float currentLinearDepth = ComputeLinearDepth(currentDepth, u_sceneView);
		const float maxPlaneDistance = max(0.015f * currentLinearDepth, 0.025f);

		const float2 virtualPointHistoryUV = prevFrameNDC.xy * 0.5f + 0.5f;

		const float2 motion = u_motionTexture.Load(uint3(pixel, 0));
		const float2 surfaceHistoryUV = uv - motion;

		const float3 currentSampleNormal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));

		float2 diffuseReprojectedUV;
		float motionBasedReprojectionConfidence;
		float3 motionReprojectedWS;
		FindTemporalReprojection(surfaceHistoryUV,
		                         currentSampleNormal,
		                         currentSampleWS,
		                         pixelSize,
		                         maxPlaneDistance,
		                         roughness,
		                         motionReprojectedWS,
		                         OUT diffuseReprojectedUV,
		                         OUT motionBasedReprojectionConfidence);

		const float diffuseLum = Luminance(diffuse.rgb);
		float2 diffuseMoments = float2(diffuseLum, Pow2(diffuseLum));
		uint diffuseHistoryLength = 1u;

		float4 motionBasedSpecular = 0.f;
		uint motionBasedSpecularHistoryLength = 1u;
		float3 motionBasedFastSpecular = 0.f;
		float2 motionBasedSpecularMoments = 0.f;

		if (motionBasedReprojectionConfidence > 0.05f)
		{
			const uint3 historyPixel = uint3(diffuseReprojectedUV * outputRes.xy, 0u);

			diffuseHistoryLength += u_historyDiffuseHistoryLengthTexture.Load(historyPixel);

			diffuseHistoryLength = min(diffuseHistoryLength, uint(motionBasedReprojectionConfidence * MAX_ACCUMULATED_FRAMES_NUM));
			float currentFrameWeight = rcp(float(diffuseHistoryLength + 1u));

			const float2 diffuseHistoryMoments = u_diffuseHistoryTemporalVarianceTexture.Load(historyPixel);
			diffuseMoments = lerp(diffuseHistoryMoments, diffuseMoments, max(currentFrameWeight, 0.02f));

			const float diffuseFrameWeight = currentFrameWeight + min(1.f - currentFrameWeight, 0.3f) * (1.f - motionBasedReprojectionConfidence);

			float4 diffuseHistory = u_diffuseHistoryTexture.SampleLevel(u_linearSampler, diffuseReprojectedUV, 0.f);
			diffuseHistory.rgb = ExposedHistoryLuminanceToCurrentExposedLuminance(diffuseHistory.rgb);

			const float4 accumulatedDiffuse  = lerp(diffuseHistory, diffuse, diffuseFrameWeight);

			u_rwDiffuseTexture[pixel]  = accumulatedDiffuse;

			// Fast history

			const float currentFrameWeightFast = max(0.3f, diffuseFrameWeight);

			const float3 diffuseFastHistory = u_diffuseFastHistoryTexture.SampleLevel(u_linearSampler, diffuseReprojectedUV, 0.f);
			u_rwDiffuseFastHistoryTexture[pixel] = lerp(diffuseFastHistory, diffuse.rgb, currentFrameWeightFast);

			motionBasedSpecular              = u_specularHistoryTexture.SampleLevel(u_linearSampler, diffuseReprojectedUV, 0.f);
			motionBasedSpecularHistoryLength = u_historyDiffuseHistoryLengthTexture.Load(historyPixel);
			motionBasedFastSpecular          = u_specularFastHistoryTexture.SampleLevel(u_linearSampler, diffuseReprojectedUV, 0.f);
			motionBasedSpecularMoments       = u_specularHistoryTemporalVarianceTexture.Load(historyPixel);
		}
		else
		{
			u_rwDiffuseFastHistoryTexture[pixel]  = diffuse.rgb;
		}

		u_diffuseHistoryLengthTexture[pixel] = diffuseHistoryLength;

		float2 specularReprojectedUV;
		float specularReprojectionConfidence;
		float3 specularReprojectedWS;
		FindTemporalReprojection(virtualPointHistoryUV,
		                         currentSampleNormal,
		                         currentSampleWS,
		                         pixelSize,
		                         maxPlaneDistance,
		                         roughness,
		                         specularReprojectedWS,
		                         OUT specularReprojectedUV,
		                         OUT specularReprojectionConfidence);

		const float specularLum = Luminance(specular.rgb);
		float2 specularMoments = float2(specularLum, Pow2(specularLum));
		uint specularHistoryLength = 1u;

		float4 virtualPointSpecular = 0.f;
		uint virtualPointSpecularHistoryLength = 1u;
		float3 virtualPointFastSpecular = 0.f;
		float2 virtualPointSpecularMoments = 0.f;

		if (specularReprojectionConfidence > 0.2f)
		{
			const uint3 historyPixel = uint3(specularReprojectedUV * outputRes.xy, 0u);

			virtualPointSpecular              = u_specularHistoryTexture.SampleLevel(u_linearSampler, diffuseReprojectedUV, 0.f);
			virtualPointSpecularHistoryLength = u_historyDiffuseHistoryLengthTexture.Load(historyPixel);
			virtualPointFastSpecular          = u_specularFastHistoryTexture.SampleLevel(u_linearSampler, diffuseReprojectedUV, 0.f);
			virtualPointSpecularMoments       = u_specularHistoryTemporalVarianceTexture.Load(historyPixel);
		}

		if(motionBasedReprojectionConfidence > 0.2f || specularReprojectionConfidence > 0.2f)
		{
			const float mbWeight = (1.f - specularReprojectionConfidence) * motionBasedReprojectionConfidence;
			const float vpWeight = specularReprojectionConfidence;
			const float rcpSumWeight = rcp(mbWeight + vpWeight);

			const float mbWeightNorm = mbWeight * rcpSumWeight;
			const float vpWeightNorm = vpWeight * rcpSumWeight;

			const float3 specularVariance = abs(Pow2(neighbourhoodInfo.specularM1.rgb) - neighbourhoodInfo.specularM2.rgb);
			const float3 specularStdDev = sqrt(specularVariance);

			const float3 clampWindow = specularStdDev * 6.f;

			const float3 specularHistoryClamped = clamp(virtualPointSpecular.rgb, neighbourhoodInfo.specularM1.rgb - clampWindow, neighbourhoodInfo.specularM1.rgb + clampWindow);

			const float parallaxCos = dot(normalize(u_sceneView.viewLocation - specularReprojectedWS), normalize(u_sceneView.viewLocation - currentSampleWS));
			const float parallaxConfidence = Pow3(saturate((parallaxCos - 0.7f) / 0.3f));

			const float unclampedWeight = parallaxConfidence * (0.6f * specularReprojectionConfidence + 0.4f * roughness);

			const float3 vpFinalSpecularHistory = lerp(specularHistoryClamped, virtualPointSpecular.rgb, unclampedWeight);
			
			const float3 specularHistory = vpFinalSpecularHistory * vpWeightNorm + motionBasedSpecular.rgb * mbWeightNorm;

			const float historyHitDist = virtualPointSpecular.w * vpWeightNorm + motionBasedSpecular.w * mbWeightNorm;

			const uint historyLength = virtualPointSpecularHistoryLength * vpWeightNorm + motionBasedSpecularHistoryLength * mbWeightNorm;

			const float2 specularHistoryMoments = virtualPointSpecularMoments * vpWeightNorm + motionBasedSpecularMoments * mbWeightNorm;

			const float3 fastHistory = virtualPointFastSpecular * vpWeightNorm + motionBasedFastSpecular * mbWeightNorm;

			const float roughnessMaxHistoryMultiplier = 1.f - 0.4f * (max(0.4f - roughness, 0.f) / 0.4f);

			specularHistoryLength = min(historyLength + 1u, uint(roughnessMaxHistoryMultiplier * parallaxConfidence * (mbWeight + vpWeight) * MAX_ACCUMULATED_FRAMES_NUM));
			float currentFrameWeight = rcp(float(specularHistoryLength + 1u));

			const float specularFrameWeight = currentFrameWeight + min(1.f - currentFrameWeight, 0.3f) * (1.f - (vpWeight + mbWeight));

			specularMoments = lerp(specularHistoryMoments, specularMoments, specularFrameWeight);

			const float3 accumulatedSpecular = lerp(specularHistory, specular.rgb, specularFrameWeight);
			const float accumulatedHitDist   = isnan(historyHitDist) ? hitDistance : lerp(historyHitDist, hitDistance, max(specularFrameWeight, 0.05f));

			u_rwSpecularTexture[pixel] = float4(accumulatedSpecular, accumulatedHitDist);

			// Fast history

			const float currentFrameWeightFast = max(0.2f, specularFrameWeight);

			u_rwSpecularFastHistoryTexture[pixel] = lerp(fastHistory, specular.rgb, currentFrameWeightFast);
		}
		else
		{
			u_rwSpecularTexture[pixel] = float4(specular.rgb, hitDistance);
			u_rwSpecularFastHistoryTexture[pixel]  = specular.rgb;
		}

		u_specularHistoryLengthTexture[pixel] = specularHistoryLength;

		u_rwSpecularTemporalVarianceTexture[pixel] = specularMoments;
		u_rwDiffuseTemporalVarianceTexture[pixel]  = diffuseMoments;
	}
}
