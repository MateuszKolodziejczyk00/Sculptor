#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"


#include "SpecularReflections/SpecularReflectionsCommon.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(SRTemporalAccumulationDS, 1)]]


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
        const float roughness = u_specularColorRoughnessTexture.Load(uint3(pixel, 0)).w;

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

        float2 historyUV = prevFrameNDC.xy * 0.5f + 0.5f;

        bool acceptReprojection = false;

        if (all(historyUV >= 0.f) && all(historyUV <= 1.f))
        {
            const float3 currentSampleNormal = u_normalsTexture.Load(uint3(pixel, 0)).xyz * 2.f - 1.f;

            const float historySampleDepth = u_historyDepthTexture.SampleLevel(u_nearestSampler, historyUV, 0.f);

            const float3 historySampleNDC = float3(historyUV * 2.f - 1.f, historySampleDepth);
            const float3 historySampleWS = NDCToWorldSpaceNoJitter(historySampleNDC, u_prevFrameSceneView);

            const float4 currentSamplePlane = float4(currentSampleNormal, -dot(currentSampleNormal, currentSampleWS));
            const float historySampleDistToCurrentSamplePlane = dot(float4(historySampleWS, 1.f), currentSamplePlane);

            if (abs(historySampleDistToCurrentSamplePlane) < 0.011f)
            {
                acceptReprojection = true;
            }
        }

        if(!acceptReprojection)
        {
            const float2 motion = u_motionTexture.Load(uint3(pixel, 0));
            
            historyUV = uv - motion;

            if(all(historyUV >= 0.f) && all(historyUV <= 1.f))
            {
                const float historySampleDepth = u_historyDepthTexture.SampleLevel(u_nearestSampler, historyUV, 0.f);
                const float3 historySampleNDC = float3(historyUV * 2.f - 1.f, historySampleDepth);
                const float3 historySampleWS = NDCToWorldSpaceNoJitter(historySampleNDC, u_prevFrameSceneView);

                if (distance(historySampleWS, currentSampleWS) < 0.02f)
                {
                    acceptReprojection = true;
                }
            }
        }

        uint historySamplesNum = 1u;
        const float lum = Luminance(luminanceAndHitDist.rgb);
        float2 moments = float2(lum, lum * lum);

        if (acceptReprojection)
        {
            const uint3 historyPixel = uint3(historyUV * outputRes.xy, 0u);

            historySamplesNum += u_historyAccumulatedSamplesNumTexture.Load(historyPixel);

            const float minHistoryWeight = lerp(0.05f, 0.3f, Pow3(1.f - roughness / GLOSSY_TRACE_MAX_ROUGHNESS));
            const float currentFrameWeight = max(minHistoryWeight, rcp(float(historySamplesNum)));
            
            const float4 historyValue = u_historyTexture.SampleLevel(u_linearSampler, historyUV, 0.0f);

            const float4 newValue = lerp(historyValue, luminanceAndHitDist, currentFrameWeight);

            u_currentTexture[pixel] = newValue;

            const float2 historyMoments = u_historyTemporalVarianceTexture.Load(historyPixel);
            moments = lerp(historyMoments, moments, currentFrameWeight);

            historySamplesNum = max(historySamplesNum, 128u);
        }

        u_accumulatedSamplesNumTexture[pixel] = historySamplesNum;
        u_temporalVarianceTexture[pixel]      = moments;
    }
}
