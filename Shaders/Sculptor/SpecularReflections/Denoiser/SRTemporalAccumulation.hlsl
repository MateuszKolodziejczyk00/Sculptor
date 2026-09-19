#include "SculptorShader.hlsli"

#ifndef USE_STABLE_BLENDS
#error "USE_STABLE_BLENDS must be defined"
#endif // USE_STABLE_BLENDS


[[shader_params(GPURenderView, VIEW)]]
[[shader_params(SRTemporalAccumulationParams, PARAMS_S_R_TEMPORAL_ACCUMULATION)]]


#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "SpecularReflections/RTGICommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"
#include "SpecularReflections/Denoiser/RTDenoising.hlsli"



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
		const float historySampleDepth = PARAMS_S_R_TEMPORAL_ACCUMULATION->historyDepthTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), reprojectedUV, 0.f);

		for(uint sampleIdx = 0; sampleIdx < HISTORY_SAMPLE_COUNT; ++sampleIdx)
		{
			const float2 sampleUV = reprojectedUV + g_historyOffsets[sampleIdx] * pixelSize;

			const float3 historySampleNDC = float3(sampleUV * 2.f - 1.f, historySampleDepth);
			const float3 historySampleWS = NDCToWorldSpaceNoJitter(historySampleNDC, VIEW->prevFrameSceneView);

			const float historySampleDistToCurrentSamplePlane = currentSamplePlane.Distance(historySampleWS);
			if (abs(historySampleDistToCurrentSamplePlane) < maxPlaneDistance)
			{
				const float3 historyNormals  = OctahedronDecodeNormal(PARAMS_S_R_TEMPORAL_ACCUMULATION->historyNormalsTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), sampleUV, 0.f));
				const float historyRoughness = PARAMS_S_R_TEMPORAL_ACCUMULATION->historyRoughnessTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), sampleUV, 0.f);

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

		const float4 specular = PARAMS_S_R_TEMPORAL_ACCUMULATION->specularTexture[sampleCoords];
		gs_specular[sampleY][sampleX] = float4(RGBToYCoCg(specular.rgb), specular.w);
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


void AccumulateSH(RTSphericalBasis aY, float2 aCoCg, RTSphericalBasis bY, float2 bCoCg, float3 normal, float alpha, out RTSphericalBasis outY, out float2 outCoCg)
{
	float aWeight = (1.f - alpha);
	float bWeight = alpha;

	const float aLum = aY.Evaluate(normal);
	const float bLum = bY.Evaluate(normal);

#if USE_STABLE_BLENDS
	aWeight /= (aLum + 5.f);
	bWeight /= (bLum + 5.f);
#endif // USE_STABLE_BLENDS

	const float rcpWeightSum = 1.f / (aWeight + bWeight);

	outY = (aY * aWeight + bY * bWeight) * rcpWeightSum;

	const float ratio = outY.val / max(bY.val, 0.0001f);
	outCoCg = bCoCg * ratio;
}


// Algorithm based on RELAX temporal accumulation: https://github.com/NVIDIAGameWorks/RayTracingDenoiser/blob/master/Shaders/Include/RELAX_TemporalAccumulation.hlsli
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void SRTemporalAccumulationCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	
	uint2 outputRes = PARAMS_S_R_TEMPORAL_ACCUMULATION->rwSpecularY_SH2.GetResolution();

	PrecacheLocalSpecularValues(int2(input.groupID.xy), int2(input.localID.xy), int2(outputRes));

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float roughness = PARAMS_S_R_TEMPORAL_ACCUMULATION->roughnessTexture.Load(uint3(pixel, 0));

		const float2 pixelSize = rcp(float2(outputRes));
		const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

		const float3 currentSampleNormal = OctahedronDecodeNormal(PARAMS_S_R_TEMPORAL_ACCUMULATION->normalsTexture.Load(uint3(pixel, 0)));
		const float3 lightDirection      = OctahedronDecodeNormal(PARAMS_S_R_TEMPORAL_ACCUMULATION->lightDirection.Load(uint3(pixel, 0)));

		float4 specular = PARAMS_S_R_TEMPORAL_ACCUMULATION->specularTexture[pixel];
		float4 diffuse  = PARAMS_S_R_TEMPORAL_ACCUMULATION->diffuseTexture[pixel];
		specular.rgb = RGBToYCoCg(specular.rgb);
		diffuse.rgb = RGBToYCoCg(diffuse.rgb);

		const RTSphericalBasis specularY = CreateRTSphericalBasis(specular.x, lightDirection);
		const float2 specularCoCg = specular.yz;
		const RTSphericalBasis diffuseY = CreateRTSphericalBasis(diffuse.x, lightDirection);
		const float2 diffuseCoCg = diffuse.yz;

		RTSphericalBasis outSpecularY;
		float2 outSpecularCoCg;
		RTSphericalBasis outDiffuseY;
		float2 outDiffuseCoCg;

		const float currentDepth = PARAMS_S_R_TEMPORAL_ACCUMULATION->depthTexture.Load(uint3(pixel, 0));
		if(currentDepth == 0.f)
		{
			return;
		}

		const NeighbourhoodInfo neighbourhoodInfo = LoadNeighbourhoodInfo(int2(input.localID.xy), int2(outputRes));

		float hitDistance = ComputeHitDistance(neighbourhoodInfo);

		const float3 currentNDC = float3(uv * 2.f - 1.f, currentDepth);
		const float3 currentSampleWS = NDCToWorldSpaceNoJitter(currentNDC, VIEW->sceneView);
		const float3 projectedReflectionWS = currentSampleWS + normalize(currentSampleWS - VIEW->sceneView.viewLocation) * hitDistance;
		const float3 prevFrameNDC = WorldSpaceToNDCNoJitter(projectedReflectionWS, VIEW->prevFrameSceneView);

		const float currentLinearDepth = ComputeLinearDepth(currentDepth, VIEW->sceneView);
		const float maxPlaneDistance = max(0.015f * currentLinearDepth, 0.025f);

		const float2 virtualPointHistoryUV = prevFrameNDC.xy * 0.5f + 0.5f;

		const float2 motion = PARAMS_S_R_TEMPORAL_ACCUMULATION->motionTexture.Load(uint3(pixel, 0));
		const float2 surfaceHistoryUV = uv - motion;

		float2 diffuseReprojectedUV;
		float motionBasedReprojectionConfidence;
		float3 motionReprojectedWS;
		FindTemporalReprojection(surfaceHistoryUV,
		                         currentSampleNormal,
		                         currentSampleWS,
		                         pixelSize,
		                         maxPlaneDistance,
		                         roughness,
		                         OUT motionReprojectedWS,
		                         OUT diffuseReprojectedUV,
		                         OUT motionBasedReprojectionConfidence);

		const float diffuseLum = diffuse.x;
		float2 diffuseMoments = float2(diffuseLum, Pow2(diffuseLum));
		uint diffuseHistoryLength = 1u;

		RTSphericalBasis motionBasedSpecularY = RawToRTSphericalBasis(0.f);
		float2 motionBasedSpecularCoCg = 0.f;
		uint motionBasedSpecularHistoryLength = 1u;
		float3 motionBasedFastSpecular = 0.f;
		float2 motionBasedSpecularMoments = 0.f;

		const uint boostedAccumulationFramesNum = 3u;

		if (motionBasedReprojectionConfidence > 0.05f)
		{
			const uint3 historyPixel = uint3(diffuseReprojectedUV * outputRes.xy, 0u);

			diffuseHistoryLength += PARAMS_S_R_TEMPORAL_ACCUMULATION->historyDiffuseHistoryLengthTexture.Load(historyPixel);

			diffuseHistoryLength = min(diffuseHistoryLength, uint(motionBasedReprojectionConfidence * MAX_ACCUMULATED_FRAMES_NUM));
			float currentFrameWeight = diffuseHistoryLength <= boostedAccumulationFramesNum ? 0.5f : rcp(float(diffuseHistoryLength + 1u));

			const float2 diffuseHistoryMoments = PARAMS_S_R_TEMPORAL_ACCUMULATION->diffuseHistoryTemporalVarianceTexture.Load(historyPixel);
			diffuseMoments = lerp(diffuseHistoryMoments, diffuseMoments, max(currentFrameWeight, 0.02f));

			const float diffuseFrameWeight = currentFrameWeight + min(1.f - currentFrameWeight, 0.3f) * (1.f - motionBasedReprojectionConfidence);

			RTSphericalBasis diffuseHistoryY = RawToRTSphericalBasis(PARAMS_S_R_TEMPORAL_ACCUMULATION->historyDiffuseY_SH2.SampleLevel(BindlessSamplers::LinearClampEdge(), diffuseReprojectedUV, 0.f));
			diffuseHistoryY = diffuseHistoryY * HistoryToCurrentExposedLuminanceFactor();
			const float4 diffSpecCoCgHistory = ExposedHistoryLuminanceToCurrentExposedLuminance(PARAMS_S_R_TEMPORAL_ACCUMULATION->historyDiffSpecCoCg.SampleLevel(BindlessSamplers::LinearClampEdge(), diffuseReprojectedUV, 0.f));

			AccumulateSH(diffuseHistoryY, diffSpecCoCgHistory.xy, diffuseY, diffuseCoCg, currentSampleNormal, diffuseFrameWeight, OUT outDiffuseY, OUT outDiffuseCoCg);

			// Fast history

			const float currentFrameWeightFast = max(0.25f, diffuseFrameWeight);

			const float3 diffuseFastHistory = PARAMS_S_R_TEMPORAL_ACCUMULATION->diffuseFastHistoryTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), diffuseReprojectedUV, 0.f);
			PARAMS_S_R_TEMPORAL_ACCUMULATION->rwDiffuseFastHistoryTexture[pixel] = lerp(diffuseFastHistory, diffuse.rgb, currentFrameWeightFast);

			motionBasedSpecularY = RawToRTSphericalBasis(PARAMS_S_R_TEMPORAL_ACCUMULATION->historySpecularY_SH2.SampleLevel(BindlessSamplers::LinearClampEdge(), diffuseReprojectedUV, 0.f));
			motionBasedSpecularY = motionBasedSpecularY * HistoryToCurrentExposedLuminanceFactor();
			motionBasedSpecularCoCg = diffSpecCoCgHistory.zw;

			motionBasedSpecularHistoryLength = PARAMS_S_R_TEMPORAL_ACCUMULATION->historyDiffuseHistoryLengthTexture.Load(historyPixel);
			motionBasedFastSpecular          = PARAMS_S_R_TEMPORAL_ACCUMULATION->specularFastHistoryTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), diffuseReprojectedUV, 0.f);
			motionBasedSpecularMoments       = PARAMS_S_R_TEMPORAL_ACCUMULATION->specularHistoryTemporalVarianceTexture.Load(historyPixel);
		}
		else
		{
			outDiffuseY    = diffuseY;
			outDiffuseCoCg = diffuseCoCg;
			PARAMS_S_R_TEMPORAL_ACCUMULATION->rwDiffuseFastHistoryTexture[pixel] = diffuse.rgb;
		}

		PARAMS_S_R_TEMPORAL_ACCUMULATION->diffuseHistoryLengthTexture[pixel] = diffuseHistoryLength;

		float2 specularReprojectedUV;
		float specularReprojectionConfidence;
		float3 specularReprojectedWS;
		FindTemporalReprojection(virtualPointHistoryUV,
		                         currentSampleNormal,
		                         currentSampleWS,
		                         pixelSize,
		                         maxPlaneDistance,
		                         roughness,
		                         OUT specularReprojectedWS,
		                         OUT specularReprojectedUV,
		                         OUT specularReprojectionConfidence);

		const float specularLum = specular.x;
		float2 specularMoments = float2(specularLum, Pow2(specularLum));
		uint specularHistoryLength = 1u;

		RTSphericalBasis virtualPointSpecularY = RawToRTSphericalBasis(0.f);
		float2 virtualPointSpecularCoCg = 0.f;
		uint virtualPointSpecularHistoryLength = 1u;
		float3 virtualPointFastSpecular = 0.f;
		float2 virtualPointSpecularMoments = 0.f;

		if (specularReprojectionConfidence > 0.2f)
		{
			const uint3 historyPixel = uint3(specularReprojectedUV * outputRes.xy, 0u);

			virtualPointSpecularY = RawToRTSphericalBasis(PARAMS_S_R_TEMPORAL_ACCUMULATION->historySpecularY_SH2.SampleLevel(BindlessSamplers::NearestClampEdge(), specularReprojectedUV, 0.f));
			virtualPointSpecularY = virtualPointSpecularY * HistoryToCurrentExposedLuminanceFactor();

			virtualPointSpecularCoCg = ExposedHistoryLuminanceToCurrentExposedLuminance(PARAMS_S_R_TEMPORAL_ACCUMULATION->historyDiffSpecCoCg.SampleLevel(BindlessSamplers::NearestClampEdge(), specularReprojectedUV, 0.f).zw);

			virtualPointSpecularHistoryLength = PARAMS_S_R_TEMPORAL_ACCUMULATION->historySpecularHistoryLengthTexture.Load(historyPixel);
			virtualPointFastSpecular          = PARAMS_S_R_TEMPORAL_ACCUMULATION->specularFastHistoryTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), specularReprojectedUV, 0.f);
			virtualPointSpecularMoments       = PARAMS_S_R_TEMPORAL_ACCUMULATION->specularHistoryTemporalVarianceTexture.Load(historyPixel);
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

			const float3 clampWindow = specularStdDev * 3.f;

			const RTSphericalBasis specularYHistoryClamped = virtualPointSpecularY;
			const float2 specularCoCgHistoryClamped = virtualPointSpecularCoCg;

			const float parallaxCos = dot(normalize(VIEW->sceneView.viewLocation - specularReprojectedWS), normalize(VIEW->sceneView.viewLocation - currentSampleWS));
			const float parallaxConfidence = Pow3(saturate((parallaxCos - 0.7f) / 0.3f));

			RTSphericalBasis specularYHistory = virtualPointSpecularY * vpWeightNorm + motionBasedSpecularY * mbWeightNorm;
			float2 specularCoCgHistory = virtualPointSpecularCoCg * vpWeightNorm + motionBasedSpecularCoCg * mbWeightNorm;

			const float specularHistoryLum = specularYHistory.Evaluate(currentSampleNormal);
			const float maxHistoryLum = neighbourhoodInfo.specularM1.x + clampWindow.x;
			const float minHistoryLum = neighbourhoodInfo.specularM1.x - clampWindow.x;
			float ratio = 1.f;
			if (maxHistoryLum > 0.f && specularHistoryLum > maxHistoryLum)
			{
				ratio = maxHistoryLum / specularHistoryLum ;
			}
			else if (minHistoryLum > 0.f && specularHistoryLum < minHistoryLum)
			{
				ratio = minHistoryLum / specularHistoryLum;
			}

			if (ratio > 0.f && !isinf(ratio))
			{
				specularYHistory = specularYHistory * ratio;
				specularCoCgHistory *= ratio;
			}

			specularCoCgHistory = clamp(specularCoCgHistory, neighbourhoodInfo.specularM1.yz - clampWindow.yz, neighbourhoodInfo.specularM1.yz + clampWindow.yz);

			const uint historyLength = virtualPointSpecularHistoryLength * vpWeightNorm + motionBasedSpecularHistoryLength * mbWeightNorm;

			const float2 specularHistoryMoments = virtualPointSpecularMoments * vpWeightNorm + motionBasedSpecularMoments * mbWeightNorm;

			const float3 fastHistory = virtualPointFastSpecular * vpWeightNorm + motionBasedFastSpecular * mbWeightNorm;

			const float roughnessMaxHistoryMultiplier = 1.f - 0.4f * (max(0.4f - roughness, 0.f) / 0.4f);

			specularHistoryLength = min(historyLength + 1u, uint(roughnessMaxHistoryMultiplier * parallaxConfidence * (mbWeight + vpWeight) * MAX_ACCUMULATED_FRAMES_NUM));

			const float currentFrameWeight = specularHistoryLength <= boostedAccumulationFramesNum ? 0.5f : rcp(float(specularHistoryLength + 1u));

			const float specularFrameWeight = currentFrameWeight + min(1.f - currentFrameWeight, 0.3f) * (1.f - (vpWeight + mbWeight));

			specularMoments = lerp(specularHistoryMoments, specularMoments, max(specularFrameWeight, 0.2f));

			AccumulateSH(specularYHistory, specularCoCgHistory, specularY, specularCoCg, currentSampleNormal, specularFrameWeight, OUT outSpecularY, OUT outSpecularCoCg);

			//// Fast history

			const float currentFrameWeightFast = max(0.2f, specularFrameWeight);

			PARAMS_S_R_TEMPORAL_ACCUMULATION->rwSpecularFastHistoryTexture[pixel] = lerp(fastHistory, specular.rgb, currentFrameWeightFast);
		}
		else
		{
			PARAMS_S_R_TEMPORAL_ACCUMULATION->rwSpecularFastHistoryTexture[pixel]  = specular.rgb;

			outSpecularY    = specularY;
			outSpecularCoCg = specularCoCg;
		}

		PARAMS_S_R_TEMPORAL_ACCUMULATION->rwSpecHitDist[pixel] = hitDistance;

		PARAMS_S_R_TEMPORAL_ACCUMULATION->rwSpecularY_SH2[pixel] = RTSphericalBasisToRaw(outSpecularY);
		PARAMS_S_R_TEMPORAL_ACCUMULATION->rwDiffuseY_SH2[pixel]  = RTSphericalBasisToRaw(outDiffuseY);
		PARAMS_S_R_TEMPORAL_ACCUMULATION->rwDiffSpecCoCg[pixel]  = float4(any(isnan(outDiffuseCoCg)) ? 0.f : outDiffuseCoCg, any(isnan(outSpecularCoCg)) ? 0.f : outSpecularCoCg);

		PARAMS_S_R_TEMPORAL_ACCUMULATION->specularHistoryLengthTexture[pixel] = specularHistoryLength;

		PARAMS_S_R_TEMPORAL_ACCUMULATION->rwSpecularTemporalVarianceTexture[pixel] = specularMoments;
		PARAMS_S_R_TEMPORAL_ACCUMULATION->rwDiffuseTemporalVarianceTexture[pixel]  = diffuseMoments;
	}
}
