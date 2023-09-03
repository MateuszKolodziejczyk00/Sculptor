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

        float currentSampleWeight = u_params.currentFrameMinWeight;
        float variance = -1.f;
        if(u_params.hasValidVarianceTexture)
        {
            variance = u_varianceTexture.SampleLevel(u_linearSampler, uv, 0.f).x;
            currentSampleWeight = clamp(1.f - variance, u_params.currentFrameMinWeight, u_params.currentFrameMaxWeight);
        }

        const float2 motion = u_motionTexture.SampleLevel(u_nearestSampler, uv, 0.f);

        const float2 historyUV = uv - motion;

        if (all(historyUV >= 0.f) && all(historyUV <= 1.f))
        {
            const float currentDepth = u_depthTexture.SampleLevel(u_nearestSampler, uv, 0.f);
            const float3 currentNDC = float3(uv * 2.f - 1.f, currentDepth);
            const float3 currentSampleWS = NDCToWorldSpace(currentNDC, u_sceneView);
        
            const float3 currentSampleNormal = u_geometryNormalsTexture.SampleLevel(u_nearestSampler, uv, 0.0f).xyz * 2.f - 1.f;

            float2 closestHistorySampleUV = historyUV;

            const float historyDepth = u_historyDepthTexture.SampleLevel(u_nearestSampler, historyUV, 0.f);
            const float3 historyNDC = float3(historyUV * 2.f - 1.f, historyDepth);
            const float3 historySampleWS = NDCToWorldSpaceNoJitter(historyNDC, u_prevFrameSceneView);
            
            const float3 historySampleNormal = u_geometryNormalsTexture.SampleLevel(u_nearestSampler, historyUV, 0.0f).xyz * 2.f - 1.f;

            const float3 offset = historySampleWS - currentSampleWS;
            
            const float maxOffset = 0.1f;
            const float minNormalDiffCos = 0.98f;
            if (dot(offset, offset) < Pow2(maxOffset) && dot(currentSampleNormal, historySampleNormal) > minNormalDiffCos)
            {
                const float4 currentValue = u_currentTexture[pixel];
                float4 historyValue = u_historyTexture.SampleLevel(u_nearestSampler, historyUV, 0.05f);
                if(u_params.hasValidVarianceTexture)
                {
                    const float clampExtent = max(variance * u_params.varianceHistoryClampMultiplier, u_params.minHistoryClampExtent);
                    historyValue = clamp(historyValue, currentValue - clampExtent, currentValue + clampExtent);
                }

                float4 newValue = lerp(historyValue, currentValue, currentSampleWeight);

                [unroll]
                for (int i = 0; i < 3; ++i)
                {
                    if (abs(newValue[i] - currentValue[i]) < 0.03f)
                    {
                        newValue[i] = currentValue[i];
                    }
                }

                u_currentTexture[pixel] = newValue;
            }
        }
    }
}
