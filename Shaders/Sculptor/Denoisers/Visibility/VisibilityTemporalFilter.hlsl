#include "SculptorShader.hlsli"

[[descriptor_set(VisibilityTemporalFilterDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Sampling.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};

#define USE_CATMULL_ROM 1
#define MAX_HISTORY_DISTANCE 0.11f
#define VARIANCE_BOOST_FRAMES_NUM 8.f
#define MAX_VARIANCE_REDUCTION_DIST 11.f


[numthreads(8, 8, 1)]
void TemporalFilterCS(CS_INPUT input)
{
	const int2 pixel = input.globalID.xy;
	
	uint2 outputRes;
	u_currentTexture.GetDimensions(outputRes.x, outputRes.y);

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float2 pixelSize = rcp(float2(outputRes));
		const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

		const float2 moments = u_spatialMomentsTexture.SampleLevel(u_nearestSampler, uv, 0.f).xx;
		const float neighbourhoodMean = moments.x;
		const float neighbourhoodVariance = abs(moments.x - Pow2(moments.y));

		if(neighbourhoodVariance == 0.f)
		{
			u_temporalMomentsTexture[pixel] = float2(neighbourhoodMean, neighbourhoodMean); // has to be either 0 or 1
			u_varianceTexture[pixel]        = 0.f;
			return;
		}

		float2 motion = u_motionTexture.SampleLevel(u_nearestSampler, uv, 0.f);

		const float2 historyUV = uv - motion;

		bool wasSampleAccepted = false;
		uint historySampleCount = 0;
debug::WriteDebugPixel(pixel, 0.f);
		
		const float currentValue = u_currentTexture[pixel];

		if (all(historyUV >= 0.f) && all(historyUV <= 1.f))
		{
			const float currentDepth = u_depthTexture.SampleLevel(u_linearSampler, uv, 0.f);
			const float3 currentNDC = float3(uv * 2.f - 1.f, currentDepth);
			const float3 currentSampleWS = NDCToWorldSpaceNoJitter(currentNDC, u_sceneView);

			const float4 prevFrameClip = mul(u_prevFrameSceneView.viewProjectionMatrixNoJitter, float4(currentSampleWS, 1.f));
			const float2 prevFrameUV = (prevFrameClip.xy / prevFrameClip.w) * 0.5f + 0.5f;
		
			const float3 currentSampleNormal = u_normalsTexture.SampleLevel(u_linearSampler, uv, 0.0f).xyz * 2.f - 1.f;

			const Plane currentSamplePlane = Plane::Create(currentSampleNormal, currentSampleWS);

			const float historySampleDepth = u_historyDepthTexture.SampleLevel(u_linearSampler, historyUV, 0.f);

			const float2 historySampleUV = round(historyUV * float2(outputRes)) * pixelSize;
			const float3 historySampleNDC = float3(historySampleUV * 2.f - 1.f, historySampleDepth);
			const float3 historySampleWS = NDCToWorldSpaceNoJitter(historySampleNDC, u_prevFrameSceneView);

			const float VdotN = max(dot(u_sceneView.viewForward, currentSampleNormal), 0.f);
			const float distanceThreshold = lerp(0.0125f, 0.033f, 1.f - VdotN);

			const float sampleDistance = currentSamplePlane.Distance(historySampleWS);

			if (sampleDistance <= distanceThreshold)
			{
				float currentFrameWeight = u_params.currentFrameDefaultWeight;
				const int2 historyPixel = round(historyUV * outputRes);
				historySampleCount = u_accumulatedSamplesNumHistoryTexture.Load(int3(historyPixel, 0));
				currentFrameWeight = max(currentFrameWeight * 0.1f, rcp(float(historySampleCount + 1)));
				debug::WriteDebugPixel(pixel, historySampleCount);

#if USE_CATMULL_ROM
				float historyValue = SampleCatmullRom(u_historyTexture, u_linearSampler, historyUV, outputRes);
#else
				float historyValue = u_historyTexture.SampleLevel(u_linearSampler, historyUV, 0.0f);
#endif // USE_CATMULL_ROM

				const float spatialStdDev = sqrt(neighbourhoodVariance);
				const float extent = 0.5f;
				const float historyMin = 0.0f;
				const float historyMax = neighbourhoodMean + spatialStdDev * extent;

				const float clampedHistoryValue = clamp(historyValue, historyMin, historyMax);

				const float sigma = 20.0f;
				const float historyDeviation = (historyValue - clampedHistoryValue) / max(spatialStdDev * sigma, 0.0001f);
				const float currentFrameWeightMultiplier = exp(-Pow2(historyDeviation) / sigma);
				currentFrameWeight *= currentFrameWeightMultiplier;

				historyValue = clampedHistoryValue;

				const float motionConfidence = 1.f - saturate(length(motion) / MAX_HISTORY_DISTANCE);

				const float historyWeight = saturate((1.0f - currentFrameWeight) * motionConfidence);
				float newValue = lerp(currentValue, historyValue, historyWeight);
				newValue = (abs(newValue - historyValue) < 0.0001f ? currentValue : newValue);

				const float currentMomentsWeight = min(0.1f, 1.f / (historySampleCount + 1));
				const float2 temporalMoments = float2(currentValue, Pow2(currentValue));
				const float2 historyTemporalMoments = u_temporalMomentsHistoryTexture.SampleLevel(u_linearSampler, historyUV, 0.f).xy;

				half2 newTemporalMoments = half2(lerp(historyTemporalMoments, temporalMoments, currentMomentsWeight));
				newTemporalMoments.x = newTemporalMoments.x == historyTemporalMoments.x ? half(temporalMoments.x) : newTemporalMoments.x;
				newTemporalMoments.y = newTemporalMoments.y == historyTemporalMoments.y ? half(temporalMoments.y) : newTemporalMoments.y;

				u_temporalMomentsTexture[pixel] = newTemporalMoments;

				float temporalVariance = abs(newTemporalMoments.y - Pow2(newTemporalMoments.x));

				u_currentTexture[pixel] = newValue;
				if(historySampleCount < VARIANCE_BOOST_FRAMES_NUM)
				{
					temporalVariance *= max(VARIANCE_BOOST_FRAMES_NUM - historySampleCount, 1.f);
					temporalVariance = max(temporalVariance, neighbourhoodVariance);
				}

				temporalVariance = max(temporalVariance - 0.015f * saturate(ComputeLinearDepth(currentDepth, u_sceneView) / MAX_VARIANCE_REDUCTION_DIST), 0.f);
				u_varianceTexture[pixel] = temporalVariance;

				wasSampleAccepted = true;
			}

		}

		const uint newSampleCount = wasSampleAccepted ? min(historySampleCount + 1u, u_params.accumulatedFramesMaxCount) : 0u;
		u_accumulatedSamplesNumTexture[pixel] = newSampleCount;

		if(!wasSampleAccepted)
		{
			u_varianceTexture[pixel]        = neighbourhoodVariance * 8.f;
			u_temporalMomentsTexture[pixel] = 0.f;
		}
	}
}
