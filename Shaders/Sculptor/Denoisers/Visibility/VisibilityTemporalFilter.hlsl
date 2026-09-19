#include "SculptorShader.hlsli"

[[shader_params(TemporalFilterShaderParams, PARAMS_VISIBILITY_TEMPORAL_FILTER)]]
[[shader_params(GPURenderView, VIEW)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Sampling.hlsli"
#include "Utils/Packing.hlsli"


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
	
	uint2 outputRes = PARAMS_VISIBILITY_TEMPORAL_FILTER->currentTexture.GetResolution();

	if(pixel.x < outputRes.x && pixel.y < outputRes.y)
	{
		const float2 pixelSize = rcp(float2(outputRes));
		const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

		const float2 moments = PARAMS_VISIBILITY_TEMPORAL_FILTER->spatialMomentsTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0.f).xx;
		const float neighbourhoodMean = moments.x;
		const float neighbourhoodVariance = abs(moments.x - Pow2(moments.y));

		if(neighbourhoodVariance == 0.f)
		{
			PARAMS_VISIBILITY_TEMPORAL_FILTER->temporalMomentsTexture[pixel] = float2(neighbourhoodMean, neighbourhoodMean); // has to be either 0 or 1
			PARAMS_VISIBILITY_TEMPORAL_FILTER->varianceTexture[pixel]        = 0.f;
			return;
		}

		float2 motion = PARAMS_VISIBILITY_TEMPORAL_FILTER->motionTexture.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0.f);

		const float2 historyUV = uv - motion;

		bool wasSampleAccepted = false;
		uint historySampleCount = 0;

		const float currentValue = PARAMS_VISIBILITY_TEMPORAL_FILTER->currentTexture[pixel];

		if (all(historyUV >= 0.f) && all(historyUV <= 1.f))
		{
			const float currentDepth = PARAMS_VISIBILITY_TEMPORAL_FILTER->depthTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), uv, 0.f);
			const float3 currentNDC = float3(uv * 2.f - 1.f, currentDepth);
			const float3 currentSampleWS = NDCToWorldSpaceNoJitter(currentNDC, VIEW->sceneView);

			const float4 prevFrameClip = mul(VIEW->prevFrameSceneView.viewProjectionMatrixNoJitter, float4(currentSampleWS, 1.f));
			const float2 prevFrameUV = (prevFrameClip.xy / prevFrameClip.w) * 0.5f + 0.5f;
		
			const float3 currentSampleNormal = OctahedronDecodeNormal(PARAMS_VISIBILITY_TEMPORAL_FILTER->normalsTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), uv, 0.0f));

			const Plane currentSamplePlane = Plane::Create(currentSampleNormal, currentSampleWS);

			const float historySampleDepth = PARAMS_VISIBILITY_TEMPORAL_FILTER->historyDepthTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), historyUV, 0.f);

			const float2 historySampleUV = round(historyUV * float2(outputRes)) * pixelSize;
			const float3 historySampleNDC = float3(historySampleUV * 2.f - 1.f, historySampleDepth);
			const float3 historySampleWS = NDCToWorldSpaceNoJitter(historySampleNDC, VIEW->prevFrameSceneView);

			const float linearDepth = ComputeLinearDepth(currentDepth, VIEW->sceneView);
			const float VdotN = max(dot(VIEW->sceneView.viewForward, currentSampleNormal), 0.f);
			const float distanceThreshold = linearDepth * lerp(0.01f, 0.025f, 1.f - VdotN);

			const float sampleDistance = currentSamplePlane.Distance(historySampleWS);

			if (sampleDistance <= distanceThreshold)
			{
				float currentFrameWeight = PARAMS_VISIBILITY_TEMPORAL_FILTER->currentFrameDefaultWeight;
				const int2 historyPixel = round(historyUV * outputRes);
				historySampleCount = PARAMS_VISIBILITY_TEMPORAL_FILTER->accumulatedSamplesNumHistoryTexture.Load(int3(historyPixel, 0));
				currentFrameWeight = max(currentFrameWeight * 0.1f, rcp(float(historySampleCount + 1)));

#if USE_CATMULL_ROM
				float historyValue = SampleCatmullRom(PARAMS_VISIBILITY_TEMPORAL_FILTER->historyTexture, BindlessSamplers::LinearClampEdge(), historyUV, outputRes);
#else
				float historyValue = PARAMS_VISIBILITY_TEMPORAL_FILTER->historyTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), historyUV, 0.0f);
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

#if USE_CATMULL_ROM
				const float2 historyTemporalMoments = saturate(SampleCatmullRom(PARAMS_VISIBILITY_TEMPORAL_FILTER->temporalMomentsHistoryTexture, BindlessSamplers::LinearClampEdge(), historyUV, outputRes));
#else
				const float2 historyTemporalMoments = PARAMS_VISIBILITY_TEMPORAL_FILTER->temporalMomentsHistoryTexture.SampleLevel(BindlessSamplers::LinearClampEdge(), historyUV, 0.0f);
#endif // USE_CATMULL_ROM

				half2 newTemporalMoments = half2(lerp(historyTemporalMoments, temporalMoments, currentMomentsWeight));
				newTemporalMoments.x = newTemporalMoments.x == historyTemporalMoments.x ? half(temporalMoments.x) : newTemporalMoments.x;
				newTemporalMoments.y = newTemporalMoments.y == historyTemporalMoments.y ? half(temporalMoments.y) : newTemporalMoments.y;

				PARAMS_VISIBILITY_TEMPORAL_FILTER->temporalMomentsTexture[pixel] = newTemporalMoments;

				float temporalVariance = abs(newTemporalMoments.y - Pow2(newTemporalMoments.x));

				PARAMS_VISIBILITY_TEMPORAL_FILTER->currentTexture[pixel] = newValue;
				if(historySampleCount < VARIANCE_BOOST_FRAMES_NUM)
				{
					temporalVariance *= max(VARIANCE_BOOST_FRAMES_NUM - historySampleCount, 1.f);
					temporalVariance = max(temporalVariance, neighbourhoodVariance);
				}

				temporalVariance = max(temporalVariance - 0.015f * saturate(ComputeLinearDepth(currentDepth, VIEW->sceneView) / MAX_VARIANCE_REDUCTION_DIST), 0.f);
				PARAMS_VISIBILITY_TEMPORAL_FILTER->varianceTexture[pixel] = temporalVariance;

				wasSampleAccepted = true;
			}

		}

		const uint newSampleCount = wasSampleAccepted ? min(historySampleCount + 1u, PARAMS_VISIBILITY_TEMPORAL_FILTER->accumulatedFramesMaxCount) : 0u;
		PARAMS_VISIBILITY_TEMPORAL_FILTER->accumulatedSamplesNumTexture[pixel] = newSampleCount;

		if(!wasSampleAccepted)
		{
			PARAMS_VISIBILITY_TEMPORAL_FILTER->varianceTexture[pixel]        = neighbourhoodVariance * 8.f;
			PARAMS_VISIBILITY_TEMPORAL_FILTER->temporalMomentsTexture[pixel] = 0.f;
		}
	}
}
