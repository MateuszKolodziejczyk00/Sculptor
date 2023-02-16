#ifndef CULLING_H
#define CULLING_H

#include "Utils/SceneViewUtils.hlsli"


bool IsSphereInFrustum(float4 frustumPlanes[4], float3 sphereCenterWS, float sphereRadius)
{
    bool isInFrustum = true;
    for(uint planeIdx = 0; planeIdx < 4; ++planeIdx)
    {
        isInFrustum = isInFrustum && dot(frustumPlanes[planeIdx], float4(sphereCenterWS, 1.f)) > -sphereRadius;
    }
    return isInFrustum;
}


bool IsConeVisible(float3 center, float radius, float3 coneAxis, float coneCutoff, float3 cameraLocation)
{
    return dot(center - cameraLocation, coneAxis) < coneCutoff * length(center - cameraLocation) + radius;
}


bool IsSphereBehindHiZ(Texture2D<float> hiZ, SamplerState hiZSampler, float2 hiZResolution, float3 sphereCenterVS, float sphereRadius, float near, float p01, float p12, out float4 aabb)
{
    if(sphereCenterVS.x - sphereRadius - near <= 0.001f)
    {
        aabb = float4(0.f, 0.f, 1.f, 1.f);
        return false;
    }

    if (GetProjectedSphereAABB(sphereCenterVS, sphereRadius, near, p01, p12, aabb))
    {
        const float width = (aabb.z - aabb.x) * hiZResolution.x;
        const float height = (aabb.w - aabb.y) * hiZResolution.y;

        const float level = floor(log2(max(width, height)));

        const float2 uv = float2((aabb.x + aabb.z) * 0.5f, (aabb.y + aabb.w) * 0.5f);
        const float sceneMinDepth = hiZ.SampleLevel(hiZSampler, uv, level).x;

        const float sphereMaxDepth = near / (sphereCenterVS.x - sphereRadius);
        // Use "<" because we use inverse depth
        // if max depth of meshlet (closest point) is smaller than min depth of scene (furthest point)
        // it means that meshlet must behind furthest point in scene
        return sphereMaxDepth < sceneMinDepth;
    }
    else
    {
        aabb = float4(0.f, 0.f, 1.f, 1.f);
    }

    return false;
}


bool IsSphereBehindHiZ(Texture2D<float> hiZ, SamplerState hiZSampler, float2 hiZResolution, float3 sphereCenterVS, float sphereRadius, float near, float p01, float p12)
{
    float4 aabb;
    return IsSphereBehindHiZ(hiZ, hiZSampler, hiZResolution, sphereCenterVS, sphereRadius, near, p01, p12, aabb);
}

#endif // CULLING_H