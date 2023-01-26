
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