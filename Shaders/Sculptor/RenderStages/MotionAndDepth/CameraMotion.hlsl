#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(CameraMotionDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void CameraMotionCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 outputRes;
    u_motion.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 uv = float2(pixel) / float2(outputRes);
        
        const float depth = u_depth.SampleLevel(u_depthSampler, uv, 0);
        const float3 worldLocation = NDCToWorldSpace(float3(uv * 2.f - 1.f, depth), u_sceneView.inverseViewProjection);
        
        const float4 prevFrameClip = mul(u_prevFrameSceneView.viewProjectionMatrix, float4(worldLocation, 1.f));
        const float2 prevFrameUV = (prevFrameClip.xy / prevFrameClip.w) * 0.5f + 0.5f;

        const float2 motion = uv - prevFrameUV;

        u_motion[pixel] = motion;
    }
}