#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(TemporalFilterDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void TemporalFilterCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_currentTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = rcp(float2(outputRes));
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

        float neighbourhoodMean = 0.f;
        float neighbourhoodVariance = 0.f;
        if(u_params.hasValidMomentsTexture)
        {
            const float2 moments = u_momentsTexture.SampleLevel(u_nearestSampler, uv, 0.f).xx;
            neighbourhoodMean = moments.x;
            neighbourhoodVariance = abs(moments.x - Pow2(moments.y));
        }

        const float2 motion = u_motionTexture.SampleLevel(u_nearestSampler, uv, 0.f);
        
        const float2 historyUV = uv - motion;

        if (all(historyUV >= 0.f) && all(historyUV <= 1.f))
        {
            const float currentDepth = u_depthTexture.SampleLevel(u_nearestSampler, uv, 0.f);
            const float3 currentNDC = float3(uv * 2.f - 1.f, currentDepth);
            const float3 currentSampleWS = NDCToWorldSpace(currentNDC, u_sceneView);
        
            const float3 currentSampleNormal = u_geometryNormalsTexture.SampleLevel(u_nearestSampler, uv, 0.0f).xyz * 2.f - 1.f;

            const float historySampleDepth = u_historyDepthTexture.SampleLevel(u_nearestSampler, historyUV, 0.f);

            const float3 historySampleNDC = float3(historyUV * 2.f - 1.f, historySampleDepth);
            const float3 historySampleWS = NDCToWorldSpace(historySampleNDC, u_prevFrameSceneView);

            const float sampleDistance = distance(historySampleWS, currentSampleWS);
            const float historySampleDepthLinear = ComputeLinearDepth(historySampleDepth, u_prevFrameSceneView);

            const float VdotN = max(dot(u_sceneView.viewForward, currentSampleNormal), 0.f);
            const float distanceThreshold = lerp(0.0225f, 0.103f, 1.f - VdotN);

            uint historySampleCount = 0;

            bool wasSampleAccepted = false;

            if (sampleDistance <= distanceThreshold)
            {
                const float depthLinearlySampled = ComputeLinearDepth(u_historyDepthTexture.SampleLevel(u_linearSampler, historyUV, 0.f), u_prevFrameSceneView);
                if(abs(depthLinearlySampled - historySampleDepthLinear) <= u_params.linearAndNearestSamplesMaxDepthDiff)
                {
                    float currentFrameWeight = u_params.currentFrameWeight;
                    if (u_params.hasValidSamplesCountTexture)
                    {
                        int2 historyPixel = round(historyUV * outputRes);
                        historySampleCount = u_accumulatedSamplesNumHistoryTexture.Load(int3(historyPixel, 0));
                        currentFrameWeight = max(currentFrameWeight, rcp(float(historySampleCount + 1)));
                    }
                
                    const float currentValue = u_currentTexture[pixel];
                    float historyValue = u_historyTexture.SampleLevel(u_linearSampler, historyUV, 0.0f);
                    if (u_params.hasValidMomentsTexture)
                    {
                        const float stdDev = sqrt(neighbourhoodVariance);
                        const float extent = 0.5f;
                        const float historyMin = 0.0f;
                        const float historyMax = neighbourhoodMean + stdDev * extent;

                        const float clampedHistoryValue = clamp(historyValue, historyMin, historyMax);

                        const float sigma = 20.f;
                        const float historyDeviation = (historyValue - clampedHistoryValue) / max(stdDev * sigma, 0.0001f);
                        const float currentFrameWeightMultiplier = exp(-Pow2(historyDeviation) / sigma);
                        currentFrameWeight *= currentFrameWeightMultiplier;

                        historyValue = clampedHistoryValue;
                    }

                    const float newValue = lerp(historyValue, currentValue, currentFrameWeight);

                    u_currentTexture[pixel] = newValue;
                    wasSampleAccepted = true;
                }
            }

            if(u_params.hasValidSamplesCountTexture)
            {
                u_accumulatedSamplesNumTexture[pixel] = wasSampleAccepted ? min(historySampleCount + 1u, 80u) : 0u;
            }
        }
    }
}