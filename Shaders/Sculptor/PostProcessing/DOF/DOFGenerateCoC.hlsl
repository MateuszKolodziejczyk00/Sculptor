#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(DOFGenerateCoCDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct CS_INPUT
{
    uint3 globalID : SV_DispatchThreadID;
};


[numthreads(8, 8, 1)]
void DOFGenerateCoCCS(CS_INPUT input)
{
    const uint2 pixel = input.globalID.xy;
    
    uint2 outputRes;
    u_cocTexture.GetDimensions(outputRes.x, outputRes.y);

    if(pixel.x < outputRes.x && pixel.y < outputRes.y)
    {
        const float2 uv = (pixel + 0.5f) / float2(outputRes);
        
        const float ndcDepth = u_depthTexture.SampleLevel(u_nearestSampler, uv, 0.f);
        const float linearDepth = ComputeLinearDepth(ndcDepth, GetNearPlane(u_sceneView.projectionMatrix));

        const float nearFieldCoC = linearDepth < u_params.nearFieldEnd ? 1.f - smoothstep(u_params.nearFieldBegin, u_params.nearFieldEnd, linearDepth) : 0.f;
        const float farFieldCoC = linearDepth > u_params.farFieldBegin ? smoothstep(u_params.farFieldBegin, u_params.farFieldEnd, linearDepth) : 0.f;
        
        u_cocTexture[pixel] = float2(nearFieldCoC, farFieldCoC);
    }
}
