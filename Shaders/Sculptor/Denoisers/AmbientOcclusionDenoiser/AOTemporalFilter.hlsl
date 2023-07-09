#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(AOTemporalFilterDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void AOTemporalFilterCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_currentAOTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 pixelSize = rcp(float2(outputRes));
        const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

        const float2 motion = u_motionTexture.SampleLevel(u_nearestSampler, uv, 0.f);

        const float2 historyUV = uv - motion;

        if (all(historyUV >= 0.f) && all(historyUV <= 1.f))
        {
            const float currentDepth = u_depthTexture.SampleLevel(u_nearestSampler, uv, 0.f);
            const float3 currentNDC = float3(uv * 2.f - 1.f, currentDepth);
            const float3 currentSampleWS = NDCToWorldSpace(currentNDC, u_sceneView);

            float2 closestHistorySampleUV = historyUV;
            float closestOffsetSq = 999999.f;
            for (int x = -1; x <= 1; ++x)
            {
                for (int y = -1; y <= 1; ++y)
                {
                    const float2 sampleUV = historyUV + float2(x, y) * pixelSize;
                    const float historyDepth = u_historyDepthTexture.SampleLevel(u_nearestSampler, sampleUV, 0.f);
                    const float3 historyNDC = float3(sampleUV * 2.f - 1.f, historyDepth);
                    const float3 historySampleWS = NDCToWorldSpace(historyNDC, u_prevFrameSceneView);

                    const float3 offset = historySampleWS - currentSampleWS;
                    if(dot(offset, offset) < closestOffsetSq)
                    {
                        closestOffsetSq = dot(offset, offset);
                        closestHistorySampleUV = sampleUV;
                    }
                }
            }

            const float maxOffset = 0.2f;
            if (closestOffsetSq < Pow2(maxOffset))
            {
                const float currentAOValue = u_currentAOTexture[pixel];
                const float historyAOValue = u_historyAOTexture.SampleLevel(u_nearestSampler, closestHistorySampleUV, 0.f);

                u_currentAOTexture[pixel] = lerp(historyAOValue, currentAOValue, 0.1f);
            }
        }
    }
}
