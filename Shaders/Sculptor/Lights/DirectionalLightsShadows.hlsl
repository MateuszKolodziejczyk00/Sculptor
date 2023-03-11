#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(TraceShadowRaysDS, 0)]]
[[descriptor_set(DirectionalLightShadowMaskDS, 1)]]


struct ShadowRayPayload
{
    bool isInShadow;
};


[shader("raygeneration")]
void GenerateShadowRaysRTG()
{
    const float3 lightDirection = float3(0.f, 0.f, -1.f);
    
    const float3 shadowRayDirection = -lightDirection;

    const uint2 pixel = DispatchRaysIndex().xy;
    const float2 uv = pixel / float2(DispatchRaysDimensions().xy);

    const float depth = u_depthTexture.SampleLevel(u_depthSampler, uv, 0);

    ShadowRayPayload payload = { true };

    if(depth > 0.f)
    {
        const float3 ndc = float3(uv * 2.f - 1.f, depth);
        const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView.inverseViewProjection);

        RayDesc rayDesc;
        rayDesc.TMin        = 0.01f;
        rayDesc.TMax        = 100.f;
        rayDesc.Origin      = worldLocation;
        rayDesc.Direction   = shadowRayDirection;

        TraceRay(u_worldAccelerationStructure,
                 RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                 0xFF,
                 0,
                 1,
                 0,
                 rayDesc,
                 payload);
    }

    u_shadowMask[pixel] = payload.isInShadow ? 0.f : 1.f;
}


[shader("miss")]
void ShadowRayRTM(inout ShadowRayPayload payload)
{
    payload.isInShadow = false;
}
