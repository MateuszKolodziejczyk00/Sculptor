#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(DirShadowsAccumulationMasksDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void AccumulateShadowsCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 outputRes;
    u_shadowMask.GetDimensions(outputRes.x, outputRes.y);
    
    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 uv = (float2(pixel) + 0.5f) / float2(outputRes);

        const float currentDepth = u_depth.SampleLevel(u_depthSampler, uv, 0);

        const float3 worldLocation = NDCToWorldSpace(float3(uv * 2.f - 1.f, currentDepth), u_sceneView.inverseViewProjection);

        const float4 prevFrameClip = mul(u_prevFrameSceneView.viewProjectionMatrix, float4(worldLocation, 1.f));
        const float2 prevFrameUV = (prevFrameClip.xy / prevFrameClip.w) * 0.5f + 0.5f;

        if (all(prevFrameUV >= 0.f) && all(prevFrameUV <= 1.f))
        {
            const float prevFrameDepth = prevFrameClip.z / prevFrameClip.w;

            const float prevFrameRenderedDepth = u_prevDepth.SampleLevel(u_depthSampler, prevFrameUV, 0);

            const float nearPlane = GetNearPlane(u_sceneView.projectionMatrix);

            const float prevFrameDepthLinear = ComputeLinearDepth(prevFrameDepth, nearPlane);
            const float prevFrameRenderedDepthLinear = ComputeLinearDepth(prevFrameRenderedDepth, nearPlane);

            const bool canAccumulateSample = abs(prevFrameDepthLinear - prevFrameRenderedDepthLinear) < u_params.maxDepthDifference;
            if (canAccumulateSample)
            {
                const float prevAccumulatedShadow = u_prevShadowMask.SampleLevel(u_shadowMaskSampler, prevFrameUV, 0);
        
                const float currentFrameShadow = u_shadowMask[pixel];

                const float exponentialAverageAlpha = u_params.exponentialAverageAlpha;
                const float accumulatedShadow = exponentialAverageAlpha * currentFrameShadow + (1.f - exponentialAverageAlpha) * prevAccumulatedShadow;
                u_shadowMask[pixel] = accumulatedShadow;
            }
        }
    }
}
