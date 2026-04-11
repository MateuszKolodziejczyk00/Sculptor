#ifndef SCREEN_SPACE_TRACER_HLSLI
#define SCREEN_SPACE_TRACER_HLSLI

#include "Utils/SceneViewUtils.hlsli"

// Based on:
// https://gist.github.com/h3r2tic/9c8356bdaefbe80b1a22ae0aaee192db
// "Rendering Tiny Glades With Entirely Too Much Ray Marching" by Tomasz Stachowiak


[[shader_struct(SSTracerData)]]


struct SSTraceResult
{
	bool isHit;
	float2 hitUV;
	float  hitT;
	float unoccludedT;
};


void ComputeClampedEndUV(in float2 startUV, in float startDepth, inout float2 endUV, inout float endDepth)
{
	const float2 dUV = endUV - startUV;
	const float dZ = endDepth - startDepth;

	float t = 1.f;
	if (dUV.x != 0.f)
	{
		if (dUV.x > 0.f)
		{
			t = min(t, (1.f - startUV.x) / dUV.x);
		}
		else
		{
			t = min(t, -startUV.x / dUV.x);
		}
	}
	if (dUV.y != 0.f)
	{
		if (dUV.y > 0.f)
		{
			t = min(t, (1.f - startUV.y) / dUV.y);
		}
		else
		{
			t = min(t, -startUV.y / dUV.y);
		}
	}

	endUV    = startUV + dUV * t;
	endDepth = startDepth + dZ * t;
}


SSTraceResult TraceScreenSpaceRay(in SSTracerData tracer, in SceneViewData sceneView, in float2 startUV, in float startDepth, in float2 endUV, in float endDepth, in float jitter, in bool allowOccluded)
{
	SSTraceResult result;
	result.isHit       = false;
	result.unoccludedT = 0.f;

#if DEBUG_SCREEN_SPACE_TRACER
	const bool debugRay = debug::IsPixelHovered(startUV * tracer.resolution, tracer.resolution);
#endif // DEBUG_SCREEN_SPACE_TRACER

	ComputeClampedEndUV(startUV, startDepth, endUV, endDepth);

	const float startZ = startDepth;
	const float endZ   = endDepth;

	const float stepsNum = tracer.stepsNum;

	const float2 dUV = endUV - startUV;
	const float dZ = endZ - startZ;
	const float dT = 1.f / stepsNum;

	const float rayThickness = 0.5f * GetNearPlane(sceneView);

	const float linearStepExponent = 1.f;

	const float offset = 0.1f;

	bool wasOccluded = false;

	for(float i = offset + jitter; i < stepsNum; i += 1.f)
	{
		const float t = pow(dT * i, linearStepExponent);

		const float2 uv = startUV + dUV * t;
		const float z = ComputeLinearDepth(startZ + dZ * t, sceneView);

		const float zNearest = tracer.linearDepth.SampleLevel(BindlessSamplers::NearestClampEdge(), uv, 0.f);
		const float zLinear  = tracer.linearDepth.SampleLevel(BindlessSamplers::LinearClampEdge(), uv, 0.f);

		const float minZ = min(zNearest, zLinear);
		const float maxZ = max(zNearest, zLinear);

		const float penetration = z - minZ;

		const bool isHit = z >= maxZ * 1.00002f && penetration < rayThickness;

#if DEBUG_SCREEN_SPACE_TRACER
	if(debugRay)
	{
		const float3 wsRay  = NDCToWorldSpace(float3(uv * 2.f - 1.f, ComputeProjectionDepth(z, sceneView)), sceneView);
		const float3 wsSurf = NDCToWorldSpace(float3(uv * 2.f - 1.f, ComputeProjectionDepth(maxZ, sceneView)), sceneView);

		DebugMarkerDefinition m;
		m.location = wsRay;
		m.size = 0.01f;
		m.color = isHit ? float3(1.f, 0.f, 0.f) : float3(0.f, 1.f, 0.f);
		GPUDebugRenderer::DrawPersistentMarker(m);
		m.location = wsSurf;
		m.color = float3(0.f, 0.f, 1.f);
		GPUDebugRenderer::DrawPersistentMarker(m);

		DebugLineDefinition lineDef;
		lineDef.begin = wsRay;
		lineDef.end   = wsSurf;
		lineDef.color = float3(0.f, 1.f, 1.f);
		GPUDebugRenderer::DrawPersistentLine(lineDef);
	}
#endif // DEBUG_SCREEN_SPACE_TRACER

		if (isHit)
		{
			result.isHit = true;
			result.hitUV = uv;
			result.hitT  = t;

			break;
		}
		else if (penetration < rayThickness)
		{
			if (!wasOccluded)
			{
				result.unoccludedT = t;
			}
		}
		else
		{
			wasOccluded = true;
			if (!allowOccluded)
			{
				break;
			}
		}
	}

#if DEBUG_SCREEN_SPACE_TRACER
	if(debugRay)
	{
		const float3 startWS = NDCToWorldSpace(float3(startUV * 2.f - 1.f, startDepth), sceneView);
		const float3 endWS   = NDCToWorldSpace(float3(endUV * 2.f - 1.f, endDepth), sceneView);

		if(result.isHit)
		{
			const float3 hitWS = lerp(startWS, endWS, result.hitT);

			DebugLineDefinition lineDef0;
			lineDef0.begin = startWS;
			lineDef0.end   = hitWS;
			lineDef0.color = float3(1.f, 1.f, 1.f);
			GPUDebugRenderer::DrawPersistentLine(lineDef0);

			DebugLineDefinition lineDef1;
			lineDef1.begin = endWS;
			lineDef1.end   = hitWS;
			lineDef1.color = float3(1.f, 0.f, 0.f);
			GPUDebugRenderer::DrawPersistentLine(lineDef1);
		}
		else
		{
			DebugLineDefinition lineDef;
			lineDef.begin = startWS;
			lineDef.end   = endWS;
			lineDef.color = float3(0.f, 1.f, 0.f);
			GPUDebugRenderer::DrawPersistentLine(lineDef);
		}

	}
#endif // DEBUG_SCREEN_SPACE_TRACER

	return result;
}


struct SSTraceResultExtended : SSTraceResult 
{
	float unoccludedDistance;
};


SSTraceResultExtended TraceScreenSpaceRay(in SSTracerData tracer, in SceneViewData sceneView, in float2 startUV, in float depth, in float3 rayDirWS, in float rayLenght, in float jitter)
{
	const float startDepth = depth;

	const float3 startNDC = float3(startUV * 2.f - 1.f, startDepth);
	const float3 startWS = NDCToWorldSpace(startNDC, sceneView);

	const Plane cameraNear = ConstructNearPlane(sceneView);
	// intersect ray with near plane to avoid artifacts from reprojection of points behind the camera
	IntersectionResult nearIntersection =  Ray::Create(startWS, rayDirWS).IntersectPlane(cameraNear);
	if (nearIntersection.IsValid())
	{
		rayLenght = min(rayLenght, nearIntersection.GetTime());
	}

	const float3 endWS = startWS + rayDirWS * rayLenght;
	const float3 endNDC = WorldSpaceToNDC(endWS, sceneView);

	const float2 endUV = endNDC.xy * 0.5f + 0.5f;
	const float endDepth = endNDC.z;

	const SSTraceResult traceResult = TraceScreenSpaceRay(tracer, sceneView, startUV, startDepth, endUV, endDepth, jitter, true);

	SSTraceResultExtended result;
	result.isHit              = traceResult.isHit;
	result.hitUV              = traceResult.hitUV;
	result.hitT               = traceResult.hitT;
	result.unoccludedT        = traceResult.unoccludedT;
	result.unoccludedDistance = traceResult.unoccludedT * rayLenght;

	return result;
}

#endif // SCREEN_SPACE_TRACER_HLSLI
