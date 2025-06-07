#ifndef AERIAL_PERSPECTIVE_HLSLI
#define AERIAL_PERSPECTIVE_HLSLI

#include "Utils/SceneViewUtils.hlsli"


float ComputeAPLinearDepth(in const AerialPerspectiveParams ap, in float depth)
{
    return ap.nearPlane * pow(ap.farPlane / ap.nearPlane, depth);
}


float3 APToNDC(in const AerialPerspectiveParams ap, in SceneViewData view, in float2 uv, in float linearDepth)
{
    return float3(uv * 2.f - 1.f, GetNearPlane(view) / linearDepth);
}


float3 APToWS(in const AerialPerspectiveParams ap, in SceneViewData view, in float2 uv, in float linearDepth)
{
    return NDCToWorldSpace(APToNDC(ap, view, uv, linearDepth), view);
}


float ComputeAPDepth(in const AerialPerspectiveParams ap, in float linearDepth)
{
    return log(linearDepth / ap.nearPlane) / log(ap.farPlane / ap.nearPlane);
}

#endif // AERIAL_PERSPECTIVE_HLSLI