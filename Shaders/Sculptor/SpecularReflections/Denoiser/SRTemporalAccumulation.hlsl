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
};


#define HISTORY_SAMPLE_COUNT 5

static const int2 g_historyOffsets[HISTORY_SAMPLE_COUNT] =
{
	int2(0, 0),
	int2(-1, 0),
	int2(1, 0),
	int2(0, 1),
	int2(0, -1)
};


[numthreads(8, 8, 1)]
void SRTemporalAccumulationCS(CS_INPUT input)
{
	const uint2 pixel = input.globalID.xy;
	
	uint2 outputRes;
	u_currentTexture.GetDimensions(outputRes.x, outputRes.y);

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		const float2 pixelSize = rcp(float2(outputRes));
		const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

		const float4 luminanceAndHitDist = u_currentTexture[pixel];

		const float currentDepth = u_depthTexture.Load(uint3(pixel, 0));
		if(currentDepth == 0.f)
		{
			return;
		}

		const float3 currentNDC = float3(uv * 2.f - 1.f, currentDepth);
		const float3 currentSampleWS = NDCToWorldSpaceNoJitter(currentNDC, u_sceneView);
		const float3 projectedReflectionWS = currentSampleWS + normalize(currentSampleWS - u_sceneView.viewLocation) * luminanceAndHitDist.w;
		const float3 prevFrameNDC = WorldSpaceToNDCNoJitter(projectedReflectionWS, u_prevFrameSceneView);

		const float currentLinearDepth = ComputeLinearDepth(currentDepth, u_sceneView);
		const float maxPlaneDistance = max(0.015f * currentLinearDepth, 0.025f);

		const float maxRoguhnessDiff = roughness * 0.1f;

		const float2 virtualPointHistoryUV = prevFrameNDC.xy * 0.5f + 0.5f;

		const float2 motion = u_motionTexture.Load(uint3(pixel, 0));
		const float2 surfaceHistoryUV = uv - motion;

		//const float surfaceReprojectionWeight = prevFrameNDC.z > 0.f ? saturate((roughness - 0.3f) * 2.8f) : 1.f;
		const float surfaceReprojectionWeight = 1.f;

		float2 historyUV = lerp(virtualPointHistoryUV, surfaceHistoryUV, surfaceReprojectionWeight);

		const float3 currentSampleNormal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(pixel, 0)));

		const Plane currentSamplePlane = Plane::Create(currentSampleNormal, currentSampleWS);

		float reprojectionConfidence = 0.f;

		if (all(historyUV >= 0.f) && all(historyUV <= 1.f))
		{
			const float historySampleDepth = u_historyDepthTexture.SampleLevel(u_nearestSampler, historyUV, 0.f);

			float2 bestHistoryUV = historyUV;

			for(uint sampleIdx = 0; sampleIdx < HISTORY_SAMPLE_COUNT; ++sampleIdx)
			{
				const float2 sampleUV = historyUV + g_historyOffsets[sampleIdx] * pixelSize;

				const float3 historySampleNDC = float3(sampleUV * 2.f - 1.f, historySampleDepth);
				const float3 historySampleWS = NDCToWorldSpaceNoJitter(historySampleNDC, u_prevFrameSceneView);

				const float historySampleDistToCurrentSamplePlane = currentSamplePlane.Distance(historySampleWS);
				if (abs(historySampleDistToCurrentSamplePlane) < maxPlaneDistance)
				{
					const float3 historyNormals  = OctahedronDecodeNormal(u_historyNormalsTexture.SampleLevel(u_linearSampler, sampleUV, 0.f));
					const float historyRoughness = u_historyRoughnessTexture.SampleLevel(u_linearSampler, sampleUV, 0.f);

					const float normalsSimilarity = dot(historyNormals, currentSampleNormal);
					if(normalsSimilarity >= 0.9f && abs(roughness - historyRoughness) < maxRoguhnessDiff)
					{
						const float3 historyViewDir = normalize(u_prevFrameSceneView.viewLocation - historySampleWS);
						const float3 currentViewDir = normalize(u_sceneView.viewLocation - currentSampleWS);
						const float parallaxAngle = dot(historyViewDir, currentViewDir);

						const bool isCenterSample = sampleIdx == 0;

						const float sampleReprojectionConfidence = saturate(1.f - 0.3f * saturate(1.f - parallaxAngle) + (isCenterSample ? 0.04f : 0.f));

						if(sampleReprojectionConfidence > reprojectionConfidence)
						{
							reprojectionConfidence = sampleReprojectionConfidence;
							bestHistoryUV = sampleUV;

							if(reprojectionConfidence > 0.99f)
							{
								break;
							}
						}
					}
				}
			}

			historyUV = bestHistoryUV;
		}

		uint historySamplesNum = 0u;
		const float lum = Luminance(luminanceAndHitDist.rgb);
		float2 moments = float2(lum, lum * lum);

		if (reprojectionConfidence > 0.05f)
		{
			const uint3 historyPixel = uint3(historyUV * outputRes.xy, 0u);

			historySamplesNum += max(u_historyAccumulatedSamplesNumTexture.Load(historyPixel), 1.f);

			const float3 movementDelta = u_sceneView.viewLocation - u_prevFrameSceneView.viewLocation;
			const float distToSample = distance(currentSampleWS, u_sceneView.viewLocation);
			const float parallaxAngle = length(movementDelta) / (distToSample * u_constants.deltaTime);

			const float3 toViewDir = (u_sceneView.viewLocation - currentSampleWS) / distToSample;
			const float dotNV = saturate(dot(currentSampleNormal, toViewDir));

			const float maxAccumulatedFrames = MAX_ACCUMULATED_FRAMES_NUM;

			historySamplesNum = min(historySamplesNum, uint(maxAccumulatedFrames));
			float currentFrameWeight = rcp(float(historySamplesNum + 1u));

			const float2 historyMoments = u_historyTemporalVarianceTexture.Load(historyPixel);
			moments = lerp(historyMoments, moments, currentFrameWeight);

			currentFrameWeight = currentFrameWeight + min(1.f - currentFrameWeight, 0.3f) * (1.f - reprojectionConfidence);
			
			float4 historyValue = u_historyTexture.SampleLevel(u_linearSampler, historyUV, 0.0f);
			historyValue.rgb = ExposedHistoryLuminanceToCurrentExposedLuminance(historyValue.rgb);

			const float4 newValue = lerp(historyValue, luminanceAndHitDist, currentFrameWeight);

			u_currentTexture[pixel] = newValue;

			// Fast history

			const float3 fastHistory = u_fastHistoryTexture.SampleLevel(u_linearSampler, historyUV, 0.f);
			const float currentFrameWeightFast = max(0.3f, currentFrameWeight);
			u_fastHistoryOutputTexture[pixel] = lerp(fastHistory, luminanceAndHitDist.rgb, currentFrameWeightFast);

			historySamplesNum = historySamplesNum + 1u;
		}
		else
		{
			u_fastHistoryOutputTexture[pixel] = luminanceAndHitDist.rgb;
		}

		u_accumulatedSamplesNumTexture[pixel]  = historySamplesNum;
		u_temporalVarianceTexture[pixel]       = moments;
		u_reprojectionConfidenceTexture[pixel] = reprojectionConfidence;
	}
}
