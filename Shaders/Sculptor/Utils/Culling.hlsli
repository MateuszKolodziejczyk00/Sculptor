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


bool IsAABBInFrustum(float4 frustumPlanes[5], float3 aabbMinWS, float3 aabbMaxWS)
{
	for(uint planeIdx = 0; planeIdx < 4; ++planeIdx)
	{
		const float3 planeNormal = frustumPlanes[planeIdx].xyz;
		const float planeDistance = frustumPlanes[planeIdx].w;

		const float3 positiveVertex = select(planeNormal > 0.f, aabbMaxWS, aabbMinWS);

		if(dot(frustumPlanes[planeIdx], float4(positiveVertex, 1.f)) < 0.f)
		{
			return false;
		}
	}
	return true;
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


bool IsSphereCenterBehindHiZ(SRVTexture2D<float> hiZ, SamplerState hiZSampler, float2 hiZResolution, float3 sphereCenterVS, float sphereRadius, float near, float p01, float p12, out float4 aabb)
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

        const float sphereFrontDepth = near / (sphereCenterVS.x - sphereRadius);
        // Use "<" because we use inverse depth
        // if max depth of sphere center is smaller than min depth of scene (furthest point)
        // it means that sphere front must behind furthest point in scene
        return sphereFrontDepth < sceneMinDepth;
    }
    else
    {
        aabb = float4(0.f, 0.f, 1.f, 1.f);
    }

    return false;
}


bool IsSphereBehindHiZ(SRVTexture2D<float> hiZ, float2 hiZResolution, float3 sphereCenterVS, float sphereRadius, float near, float p01, float p12, out float4 aabb)
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
        const float sceneMinDepth = hiZ.SampleLevel(BindlessSamplers::LinearMinClampEdge(), uv, level).x;

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


bool IsSphereBehindHiZ(SRVTexture2D<float> hiZ, float2 hiZResolution, float3 sphereCenterVS, float sphereRadius, float near, float p01, float p12)
{
    float4 aabb;
    return IsSphereBehindHiZ(hiZ, hiZResolution, sphereCenterVS, sphereRadius, near, p01, p12, aabb);
}


bool IsSphereBehindHiZOrthographic(SRVTexture2D<float> hiZ, float2 hiZResolution, float3 sphereCenterVS, float sphereRadius, float4x4 projection)
{
    const float2 centerCS = float2(projection[0][1] * sphereCenterVS.y + projection[0][3], projection[1][2] * sphereCenterVS.z + projection[1][3]);
    const float2 extentCS = sphereRadius * abs(float2(projection[0][1], projection[1][2]));

    const float4 aabb = saturate(float4(centerCS - extentCS, centerCS + extentCS) * 0.5f + 0.5f);

    const float2 sizeTexels = (aabb.zw - aabb.xy) * hiZResolution;
    const float level = ceil(log2(max(sizeTexels.x, sizeTexels.y)));

    const float sceneMinDepth = hiZ.SampleLevel(BindlessSamplers::LinearMinClampEdge(), (aabb.xy + aabb.zw) * 0.5f, level).x;

    const float sphereMaxDepth = projection[2][0] * (sphereCenterVS.x - sphereRadius) + projection[2][3];
    return sphereMaxDepth < sceneMinDepth;
}


struct HiZCullingProcessor
{
	static HiZCullingProcessor Create(in SRVTexture2D<float> inHiZ, in float2 inRes, in SceneViewData inView)
	{
		HiZCullingProcessor processor;
		processor.hiZTexture       = inHiZ;
		processor.hiZRes           = inRes;
		return processor;
	}

	bool DoCulling(in SceneViewData inView, in Sphere sphere)
	{
		sphere.center = mul(inView.viewMatrix, float4(sphere.center, 1.f)).xyz;

		if (inView.IsOrthographic())
		{
			return !IsSphereBehindHiZOrthographic(hiZTexture, hiZRes, sphere.center, sphere.radius, inView.projectionMatrix);
		}
		else
		{
			return !IsSphereBehindHiZ(hiZTexture, hiZRes, sphere.center, sphere.radius, inView.projectionMatrix[2][3], inView.projectionMatrix[0][1], inView.projectionMatrix[1][2]);
		}
	}

	SRVTexture2D<float> hiZTexture;
	float2 hiZRes;
};

#endif // CULLING_H
