#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(SRTemporalAccumulationDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/Denoiser/SRDenoisingCommon.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
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

		if(roughness > GLOSSY_TRACE_MAX_ROUGHNESS)
		{
			return;
		}

		const float2 pixelSize = rcp(float2(outputRes));
		const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

		const float4 luminanceAndHitDist = u_currentTexture[pixel];

		const float currentDepth = u_depthTexture.Load(uint3(pixel, 0));
		const float3 currentNDC = float3(uv * 2.f - 1.f, currentDepth);
		const float3 currentSampleWS = NDCToWorldSpaceNoJitter(currentNDC, u_sceneView);
		const float3 projectedReflectionWS = currentSampleWS + normalize(currentSampleWS - u_sceneView.viewLocation) * luminanceAndHitDist.w;
		const float3 prevFrameNDC = WorldSpaceToNDCNoJitter(projectedReflectionWS, u_prevFrameSceneView);

		const float currentLinearDepth = ComputeLinearDepth(currentDepth, u_sceneView);
		const float maxPlaneDistance = max(0.015f * currentLinearDepth, 0.025f);

		const float maxRoguhnessDiff = roughness * 0.1f;

		float2 historyUV = prevFrameNDC.xy * 0.5f + 0.5f;

		float reprojectionConfidence = 0.f;

		const float3 currentSampleNormal = normalize(u_normalsTexture.Load(uint3(pixel, 0)).xyz * 2.f - 1.f);

		const Plane currentSamplePlane = Plane::Create(currentSampleNormal, currentSampleWS);

		if (all(historyUV >= 0.f) && all(historyUV <= 1.f))
		{
			const float historySampleDepth = u_historyDepthTexture.SampleLevel(u_nearestSampler, historyUV, 0.f);
			const float3 historySampleNDC = float3(historyUV * 2.f - 1.f, historySampleDepth);
			const float3 historySampleWS = NDCToWorldSpaceNoJitter(historySampleNDC, u_prevFrameSceneView);

			const float historySampleDistToCurrentSamplePlane = currentSamplePlane.Distance(historySampleWS);
			if (abs(historySampleDistToCurrentSamplePlane) < maxPlaneDistance)
			{
				const float3 historyNormals  = u_historyNormalsTexture.SampleLevel(u_linearSampler, historyUV, 0.f).xyz * 2.f - 1.f;
				const float historyRoughness = u_historyRoughnessTexture.SampleLevel(u_linearSampler, historyUV, 0.f);

				const float normalsSimilarity = dot(historyNormals, currentSampleNormal);
				if(normalsSimilarity >= 0.996f && abs(roughness - historyRoughness) < maxRoguhnessDiff)
				{
					const float3 historyViewDir = normalize(u_prevFrameSceneView.viewLocation - historySampleWS);
					const float3 currentViewDir = normalize(u_sceneView.viewLocation - currentSampleWS);
					const float parallaxAngle = dot(historyViewDir, currentViewDir);
					const float parallaxFilterStrength = 1.f;

					reprojectionConfidence = 1.f - 0.2f * saturate(parallaxAngle * parallaxFilterStrength) * (1.f - roughness);
				}
			}
		}

		if(reprojectionConfidence < 1.f)
		{
			const float2 motion = u_motionTexture.Load(uint3(pixel, 0));
			
			const float2 surfaceHistoryUV = uv - motion;

			if(all(surfaceHistoryUV >= 0.f) && all(surfaceHistoryUV <= 1.f))
			{
				const float historySampleDepth = u_historyDepthTexture.SampleLevel(u_nearestSampler, historyUV, 0.f);
				const float3 historySampleNDC = float3(historyUV * 2.f - 1.f, historySampleDepth);
				const float3 historySampleWS = NDCToWorldSpaceNoJitter(historySampleNDC, u_prevFrameSceneView);
			
				const float historySampleDistToCurrentSamplePlane = currentSamplePlane.Distance(historySampleWS);
				if (abs(historySampleDistToCurrentSamplePlane) < maxPlaneDistance)
				{
					const float motionLength = length(motion);
					if(IsNearlyZero(motionLength))
					{
						reprojectionConfidence = 1.f;
						historyUV              = surfaceHistoryUV;
					}
					const float3 historyNormal = u_historyNormalsTexture.SampleLevel(u_linearSampler, historyUV, 0.f).xyz * 2.f - 1.f;
					const float normalsSimilarity = dot(historyNormal, currentSampleNormal);
					const float historyRoughness = u_historyRoughnessTexture.SampleLevel(u_linearSampler, historyUV, 0.f);
					const float roughnessDiff = abs(roughness - historyRoughness);

					if(normalsSimilarity >= 0.95f && abs(roughness - historyRoughness) < maxRoguhnessDiff * 3.f)
					{
						const float maxMotionLength = 0.11f * roughness;
						const float confidence = 0.3f + 0.7f * saturate(1.f - motionLength / (maxMotionLength + 0.000001f));

						if(confidence > reprojectionConfidence)
						{
							reprojectionConfidence = confidence;
							historyUV              = surfaceHistoryUV;
						}
					}
				}
			}
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
			currentFrameWeight = currentFrameWeight + (1.f - currentFrameWeight) * (1.f - reprojectionConfidence);

			const float2 historyMoments = u_historyTemporalVarianceTexture.Load(historyPixel);
			moments = lerp(historyMoments, moments, currentFrameWeight);
			
			float4 historyValue = u_historyTexture.SampleLevel(u_linearSampler, historyUV, 0.0f);
			historyValue.rgb = ExposedHistoryLuminanceToCurrentExposedLuminance(historyValue.rgb);

			const float4 newValue = lerp(historyValue, luminanceAndHitDist, currentFrameWeight);

			u_currentTexture[pixel] = newValue;

			// Fast history

			const float3 fastHistory = u_fastHistoryTexture.SampleLevel(u_linearSampler, historyUV, 0.f);
			float currentFrameWeightFast = max(rcp(float(min(historySamplesNum + 1u, FAST_HISTORY_MAX_ACCUMULATED_FRAMES_NUM))), currentFrameWeight);
			u_fastHistoryOutputTexture[pixel] = lerp(fastHistory, luminanceAndHitDist.rgb, currentFrameWeightFast);

			historySamplesNum = historySamplesNum + 1u;
		}
		else
		{
			u_fastHistoryOutputTexture[pixel] = 0.f;
		}

		u_accumulatedSamplesNumTexture[pixel]  = historySamplesNum;
		u_temporalVarianceTexture[pixel]       = moments;
		u_reprojectionConfidenceTexture[pixel] = reprojectionConfidence;
	}
}
