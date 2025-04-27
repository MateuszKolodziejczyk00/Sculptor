#ifndef SCENE_VIEW_UTILS_H
#define SCENE_VIEW_UTILS_H

#include "Utils/Shapes.hlsli"

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


template<typename TDepthType>
TDepthType ComputeHWDepth(TDepthType linearDepth, in SceneViewData sceneView)
{
	return GetNearPlane(sceneView) / linearDepth;
}


template<typename TDepthType>
TDepthType ComputeLinearDepthDiff(TDepthType invDepth0, TDepthType invDepth1, in SceneViewData sceneView)
{
	return GetNearPlane(sceneView) * abs(invDepth0 - invDepth1);
}


template<typename TDepthType>
TDepthType ComputeProjectionDepth(TDepthType linearDepth, in SceneViewData sceneView)
{
	return GetNearPlane(sceneView) / linearDepth;
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


float3 NDCToViewSpaceNoJitter(in float3 ndc, in SceneViewData sceneView)
{
	const float4 viewSpace = mul(sceneView.inverseProjectionNoJitter, float4(ndc, 1.f));
	return viewSpace.xyz / viewSpace.w;
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


float3 ViewSpaceToNDCNoJitter(in float3 vs, in SceneViewData sceneView)
{
	const float4 ndc = mul(sceneView.projectionMatrixNoJitter, float4(vs, 1.f));
	return ndc.xyz / ndc.w;
}


float3 ViewSpaceToNDC(in float3 vs, in SceneViewData sceneView)
{
	const float4 ndc = mul(sceneView.projectionMatrix, float4(vs, 1.f));
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


float3 UnnormalizedRayDirFromProjection(in float4x4 projMat, in float2 uv)
{
	const float2 xy = uv * 2.f - 1.f;
	const float x = 1.f;
	const float y = xy.x / (projMat[0][1] - projMat[0][0]);
	const float z = xy.y / (projMat[1][2] - projMat[1][0]);
	return float3(x, y, z);
}


float3 ComputeViewRayDirectionVS(in SceneViewData sceneView, in float2 uv)
{
	return normalize(UnnormalizedRayDirFromProjection(sceneView.projectionMatrix, uv));
}


float3 LinearDepthToWS(in SceneViewData sceneView, in float2 ndc, in float linearDepth)
{
	const float3 rayDir = (sceneView.viewForward + sceneView.upForLinearReconstruction * ndc.y) + sceneView.rightForLinearReconstruction * ndc.x;
	return sceneView.viewLocation + rayDir * linearDepth;
}


float3 ComputeViewRayDirectionWS(in SceneViewData sceneView, in float2 uv)
{
	float3 dir = UnnormalizedRayDirFromProjection(sceneView.projectionMatrix, uv);
	return normalize(mul(sceneView.inverseView, float4(dir, 0.f))).xyz;
}


Ray CreateViewRayVS(in SceneViewData sceneView, in float2 uv)
{
	return Ray::Create(0.f, ComputeViewRayDirectionVS(sceneView, uv));
}


Ray CreateViewRayWS(in SceneViewData sceneView, in float2 uv)
{
	return Ray::Create(sceneView.viewLocation, ComputeViewRayDirectionWS(sceneView, uv));
}


float3 ComputeViewRayDirectionVSNoJitter(in SceneViewData sceneView, in float2 uv)
{
	return normalize(UnnormalizedRayDirFromProjection(sceneView.projectionMatrixNoJitter, uv));
}


float3 ComputeViewRayDirectionWSNoJitter(in SceneViewData sceneView, in float2 uv)
{
	const float3 dir = UnnormalizedRayDirFromProjection(sceneView.projectionMatrixNoJitter, uv);
	return normalize(mul(sceneView.inverseView, float4(dir, 0.f))).xyz;
}


Ray CreateViewRayVSNoJitter(in SceneViewData sceneView, in float2 uv)
{
	return Ray::Create(0.f, ComputeViewRayDirectionVSNoJitter(sceneView, uv));
}


Ray CreateViewRayWSNoJitter(in SceneViewData sceneView, in float2 uv)
{
	return Ray::Create(sceneView.viewLocation, ComputeViewRayDirectionWSNoJitter(sceneView, uv));
}

#ifdef DS_RenderViewDS

float GetViewExposure()
{
	return u_viewExposure.exposure;
}


float3 LuminanceToExposedLuminance(float3 linearLuminance)
{
	return linearLuminance * u_viewExposure.exposure;
}

float3 ExposedLuminanceToLuminance(float3 exposedLuminance)
{
	return exposedLuminance * u_viewExposure.rcpExposure;
}

float3 HistoryLuminanceToExposedLuminance(float3 linearLuminance)
{
	return linearLuminance * u_viewExposure.exposureLastFrame;
}

float3 HistoryExposedLuminanceToLuminance(float3 exposedLuminance)
{
	return exposedLuminance * u_viewExposure.rcpExposureLastFrame;
}

float3 ExposedHistoryLuminanceToCurrentExposedLuminance(float3 exposedLuminance)
{
	return exposedLuminance * (u_viewExposure.exposure / u_viewExposure.exposureLastFrame);
}

float ComputeLuminanceFromEC(float EC)
{
	return pow(2.f, u_viewExposure.EV100 + EC);
}

float GetAverageLogLuminance()
{
	return u_viewExposure.averageLogLuminance;
}

#endif // DS_RenderViewDS

#endif // SCENE_VIEW_UTILS_H
