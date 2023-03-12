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
    const uint2 pixel = DispatchRaysIndex().xy;

    if(!u_params.enableShadows)
    {
        u_shadowMask[pixel] = 1.f;
        return;
    }
    
    const float2 uv = pixel / float2(DispatchRaysDimensions().xy);
    const float depth = u_depthTexture.SampleLevel(u_depthSampler, uv, 0);

    ShadowRayPayload payload = { true };

    if(depth > 0.f)
    {
        const float2 noise = Random(uv + u_params.time);
        const float3 shadowRayDirection = VectorInCone(-u_params.lightDirection, u_params.shadowRayConeAngle, noise);

        const float3 ndc = float3(uv * 2.f - 1.f, depth);
        const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView.inverseViewProjection);

        RayDesc rayDesc;
        rayDesc.TMin        = u_params.minTraceDistance;
        rayDesc.TMax        = u_params.maxTraceDistance;
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
