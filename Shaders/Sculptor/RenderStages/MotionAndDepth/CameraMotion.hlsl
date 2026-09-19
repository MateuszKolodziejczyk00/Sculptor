#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(CameraMotionConstants, PARAMS_CAMERA_MOTION)]]

#include "Utils/SceneViewUtils.hlsli"


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void CameraMotionCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;

    uint2 outputRes = PARAMS_CAMERA_MOTION->motion.GetResolution();

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 uv = (float2(pixel) + 0.5f) / float2(outputRes);
        
        const float maxDepth = 3000.f;
        const float depth = max(PARAMS_CAMERA_MOTION->depth.SampleLevel(BindlessSamplers::LinearMinClampEdge(), uv, 0), ComputeHWDepth(maxDepth, VIEW->sceneView));
        const float3 worldLocation = NDCToWorldSpaceNoJitter(float3(uv * 2.f - 1.f, depth), VIEW->sceneView);
        
        const float4 prevFrameClip = mul(VIEW->prevFrameSceneView.viewProjectionMatrixNoJitter, float4(worldLocation, 1.f));
        const float2 prevFrameUV = (prevFrameClip.xy / prevFrameClip.w) * 0.5f + 0.5f;

        const float2 motion = uv - prevFrameUV;

        PARAMS_CAMERA_MOTION->motion[pixel] = motion;
    }
}
