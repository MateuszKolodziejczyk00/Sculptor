#ifndef CULLING_H
#define CULLING_H

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Shapes.hlsli"


bool IsSphereInFrustum(float4 frustumPlanes[5], float3 sphereCenterWS, float sphereRadius)
{
    bool isInFrustum = true;
    for(uint planeIdx = 0; planeIdx < 5; ++planeIdx)
    {
        isInFrustum = isInFrustum && dot(frustumPlanes[planeIdx], float4(sphereCenterWS, 1.f)) > -sphereRadius;
    }
    return isInFrustum;
}


int ToInt8Value(uint bits)
{
	return int(bits & 127) - int(bits & 128);
}


void UnpackConeAxisAndCutoff(uint packedData, out float3 coneAxis, out float coneCutoff)
{
	coneCutoff = float(ToInt8Value(packedData >> 24)) / 127.f;
	coneAxis.x = float(ToInt8Value(packedData >> 16)) / 127.f;
	coneAxis.y = float(ToInt8Value(packedData >>  8)) / 127.f;
	coneAxis.z = float(ToInt8Value(packedData      )) / 127.f;
}


bool IsConeVisible(float3 center, float radius, float3 coneAxis, float coneCutoff, float3 cameraLocation)
{
    return dot(center - cameraLocation, coneAxis) < coneCutoff * length(center - cameraLocation) + radius;
}


bool IsSphereCenterBehindHiZ(Texture2D<float> hiZ, SamplerState hiZSampler, float2 hiZResolution, float3 sphereCenterVS, float sphereRadius, float near, float p01, float p12, out float4 aabb)
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

        const float level = ceil(log2(max(width, height)));

        const float2 uv = float2((aabb.x + aabb.z) * 0.5f, (aabb.y + aabb.w) * 0.5f);
        const float sceneMinDepth = hiZ.SampleLevel(hiZSampler, uv, level).x;

        const float sphereCenterDepth = near / (sphereCenterVS.x);
        // Use "<" because we use inverse depth
        // if max depth of sphere center is smaller than min depth of scene (furthest point)
        // it means that sphere center must behind furthest point in scene
        return sphereCenterDepth < sceneMinDepth;
    }
    else
    {
        aabb = float4(0.f, 0.f, 1.f, 1.f);
    }

    return false;
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

        const float level = ceil(log2(max(width, height)));

        const float2 uv = float2((aabb.x + aabb.z) * 0.5f, (aabb.y + aabb.w) * 0.5f);
        const float sceneMinDepth = hiZ.SampleLevel(hiZSampler, uv, level).x;

        const float sphereMaxDepth = near / (sphereCenterVS.x - sphereRadius);
        // Use "<" because we use inverse depth
        // if max depth of sphere (closest point) is smaller than min depth of scene (furthest point)
        // it means that sphere must behind furthest point in scene
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


class HiZCullingProcessor
{
	static HiZCullingProcessor Create(in Texture2D<float> inHiZ, in float2 inRes, in SamplerState inSampler, in SceneViewData inView)
	{
		HiZCullingProcessor processor;
		processor.hiZTexture = inHiZ;
		processor.hiZRes     = inRes;
		processor.hiZSampler = inSampler;
		processor.nearPlane  = GetNearPlane(inView);
		processor.p01        = inView.projectionMatrix[0][1];
		processor.p12        = inView.projectionMatrix[1][2];
		processor.viewMatrix = inView.viewMatrix;
		return processor;
	}

	bool DoCulling(in Sphere sphere)
	{
		sphere.center = mul(viewMatrix, float4(sphere.center, 1.f)).xyz;
		return !IsSphereBehindHiZ(hiZTexture, hiZSampler, hiZRes, sphere.center, sphere.radius, nearPlane, p01, p12);
	}

	Texture2D<float> hiZTexture;
	SamplerState     hiZSampler;

	float2 hiZRes;

	// View parameters
	float p01;
	float p12;
	float nearPlane;

	float4x4 viewMatrix;
};

#endif // CULLING_H