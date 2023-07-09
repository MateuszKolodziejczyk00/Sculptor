#include "SculptorShader.hlsli"
#include "Utils/SceneViewUtils.hlsli"

[[descriptor_set(RTAOTraceRaysDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]


struct AORayPayload
{
    bool isMiss;
};


[shader("raygeneration")]
void GenerateAmbientOcclusionRaysRTG()
{
    const uint2 pixel = DispatchRaysIndex().xy;

    const float2 uv = (pixel + 0.5f) / float2(DispatchRaysDimensions().xy);
    const float depth = u_depthTexture.SampleLevel(u_nearestSampler, uv, 0);

    float ao = 1.f;

    if(depth > 0.f)
    {

        AORayPayload payload = { false };

        const float3 ndc = float3(uv * 2.f - 1.f, depth);
        const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

        const float3 normal = u_geometryNormalsTexture.SampleLevel(u_nearestSampler, uv, 0) * 2.f - 1.f;
        const float3 tangent = dot(normal, UP_VECTOR) > 0.9f ? cross(normal, RIGHT_VECTOR) : cross(normal, UP_VECTOR);
        const float3 bitangent = cross(normal, tangent);

        const float2 random = float2(Random(float2(pixel) + u_rtaoParams.randomSeed.xy), Random(float2(pixel.yx) + u_rtaoParams.randomSeed.yx));

        const float3x3 tangentSpace = transpose(float3x3(tangent, bitangent, normal));

        float pdf = 0.f;
        const float3 rayDirection = RandomVectorInCosineWeightedHemisphere(tangentSpace, random, OUT pdf);

        RayDesc rayDesc;
        rayDesc.TMin        = u_rtaoParams.raysMinHitDistance;
        rayDesc.TMax        = u_rtaoParams.raysLength;
        rayDesc.Origin      = worldLocation;
        rayDesc.Direction   = rayDirection;

        TraceRay(u_worldAccelerationStructure,
                 RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                 0xFF,
                 0,
                 1,
                 0,
                 rayDesc,
                 payload);

        ao = payload.isMiss ? 1.f : 0.f;
    }

    u_ambientOcclusionTexture[pixel] = ao;
}


[shader("miss")]
void AmbientOcclusionRTM(inout AORayPayload payload)
{
    payload.isMiss = true;
}
