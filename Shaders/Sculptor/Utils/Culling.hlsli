
bool IsSphereInFrustum(float4 frustumPlanes[4], float3 sphereCenterWS, float sphereRadius)
{
    bool isInFrustum = true;
    for(uint planeIdx = 0; planeIdx < 4; ++planeIdx)
    {
        isInFrustum = isInFrustum && dot(frustumPlanes[planeIdx], float4(sphereCenterWS, 1.f)) > -sphereRadius;
    }
    return isInFrustum;
}