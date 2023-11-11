#ifndef SCENE_VIEW_UTILS_H
#define SCENE_VIEW_UTILS_H

[[shader_struct(SceneViewData)]]

float GetNearPlane(in SceneViewData sceneView)
{
    return sceneView.projectionMatrix[2][3];
}


template<typename TDepthType>
TDepthType ComputeLinearDepth(TDepthType depth, in SceneViewData sceneView)
{
    return GetNearPlane(sceneView) / depth;
}


// Based on: 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere. Michael Mara, Morgan McGuire. 2013
bool GetProjectedSphereAABB(float3 viewSpaceCenter, float radius, float znear, float P01, float P12, out float4 aabb)
{
    if (viewSpaceCenter.x < radius + znear)
    {
        return false;
    }

    float3 cr = viewSpaceCenter * radius;
    float czr2 = viewSpaceCenter.x * viewSpaceCenter.x - radius * radius;

    float vx = sqrt(viewSpaceCenter.y * viewSpaceCenter.y + czr2);
    float maxx = (vx * viewSpaceCenter.y + cr.x) / (vx * viewSpaceCenter.x + cr.y);
    float minx = (vx * viewSpaceCenter.y - cr.x) / (vx * viewSpaceCenter.x - cr.y);

    float vy = sqrt(viewSpaceCenter.z * viewSpaceCenter.z + czr2);
    float maxy = (vy * viewSpaceCenter.z - cr.x) / (vy * viewSpaceCenter.x + cr.z);
    float miny = (vy * viewSpaceCenter.z + cr.x) / (vy * viewSpaceCenter.x - cr.z);

    aabb = float4(minx * P01, miny * P12, maxx * P01, maxy * P12);
    // clip space -> uv space
    aabb = aabb * 0.5f + 0.5f;

    return true;
}


float3 NDCToViewSpace(in float3 ndc, in SceneViewData sceneView)
{
    const float4 viewSpace = mul(sceneView.inverseProjection, float4(ndc, 1.f));
    return viewSpace.xyz / viewSpace.w;
}


float3 NDCToWorldSpaceNoJitter(in float3 ndc, in SceneViewData sceneView)
{
    const float4 world = mul(sceneView.inverseViewProjectionNoJitter, float4(ndc, 1.f));
    return world.xyz / world.w;
}


float3 NDCToWorldSpace(in float3 ndc, in SceneViewData sceneView)
{
    const float4 world = mul(sceneView.inverseViewProjection, float4(ndc, 1.f));
    return world.xyz / world.w;
}


float3 WorldSpaceToNDCNoJitter(in float3 worldLocation, in SceneViewData sceneView)
{
    const float4 ndc = mul(sceneView.viewProjectionMatrixNoJitter, float4(worldLocation, 1.f));
    return ndc.xyz / ndc.w;
}


float3 WorldSpaceToNDC(in float3 worldLocation, in SceneViewData sceneView)
{
    const float4 ndc = mul(sceneView.viewProjectionMatrix, float4(worldLocation, 1.f));
    return ndc.xyz / ndc.w;
}


void ComputeShadowProjectionParams(float near, float far, out float p20, out float p23)
{
    p20 = -near / (far - near);
    p23 = -far * p20;
}


float ComputeShadowLinearDepth(float ndcDepth, float p20, float p23)
{
    return p23 / (ndcDepth - p20);
}


float ComputeShadowNDCDepth(float linearDepth, float p20, float p23)
{
    return (p20 * linearDepth + p23) / linearDepth;
}


float3 ComputeViewRayDirection(in SceneViewData sceneView, in float2 uv)
{
    const float2 xy = uv * 2.f - 1.f;
    const float x = 1.f;
    const float y = xy.x / sceneView.projectionMatrix[0][1];
    const float z = xy.y / sceneView.projectionMatrix[1][2];
    return normalize(mul(sceneView.inverseView, float4(x, y, z, 0.f))).xyz;

}

#endif // SCENE_VIEW_UTILS_H
