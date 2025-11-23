#ifndef SCREEN_SPACE_TRACER_HLSLI
#define SCREEN_SPACE_TRACER_HLSLI

#include "Utils/SceneViewUtils.hlsli"

// Based on:
// https://gist.github.com/h3r2tic/9c8356bdaefbe80b1a22ae0aaee192db
// "Rendering Tiny Glades With Entirely Too Much Ray Marching" by Tomasz Stachowiak


struct ScreenSpaceTracerContext
{
	SceneViewData    sceneView;
	Texture2D<float> linearDepth;
	SamplerState     linearSampler;
	SamplerState     nearestSampler;
	
	uint2 resolution;
	uint  stepsNum;
};


struct SSTraceResult
{
	bool isHit;
	float2 hitUV;
	float  hitT;
};


SSTraceResult TraceSSShadowRay(in ScreenSpaceTracerContext context, in float2 startUV, in float startDepth, in float2 endUV, in float endDepth, in float jitter)
{
	SSTraceResult result;
	result.isHit = false;

#if DEBUG_SCREEN_SPACE_TRACER
	const bool debugRay = debug::IsPixelHovered(startUV * context.resolution, context.resolution);
#endif // DEBUG_SCREEN_SPACE_TRACER

	const float startZ = startDepth;
	const float endZ   = endDepth;

	const float stepsNum = context.stepsNum;

	const float2 dUV = endUV - startUV;
	const float dZ = endZ - startZ;
	const float dT = 1.f / stepsNum;

	const float rayThickness = 0.5f * GetNearPlane(context.sceneView);

	const float linearStepExponent = 1.f;

	for(float i = 0.1f + jitter; i < stepsNum; i += 1.f)
	{
		const float t = pow(dT * i, linearStepExponent);

		const float2 uv = startUV + dUV * t;
		const float z = ComputeLinearDepth(startZ + dZ * t, context.sceneView);

		if(any(saturate(uv) != uv))
		{
			break;
		}

		const float zNearest = context.linearDepth.SampleLevel(context.nearestSampler, uv, 0.f);
		const float zLinear  = context.linearDepth.SampleLevel(context.linearSampler, uv, 0.f);

		const float minZ = min(zNearest, zLinear);
		const float maxZ = max(zNearest, zLinear);

		const float penetration = z - minZ;

		const bool isHit = z >= maxZ * 1.00002f && penetration < rayThickness;

#if DEBUG_SCREEN_SPACE_TRACER
	if(debugRay)
	{
		const float3 wsRay  = NDCToWorldSpace(float3(uv * 2.f - 1.f, ComputeProjectionDepth(z, context.sceneView)), context.sceneView);
		const float3 wsSurf = NDCToWorldSpace(float3(uv * 2.f - 1.f, ComputeProjectionDepth(maxZ, context.sceneView)), context.sceneView);

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
	}

#if DEBUG_SCREEN_SPACE_TRACER
	if(debugRay)
	{
		const float3 startWS = NDCToWorldSpace(float3(startUV * 2.f - 1.f, startDepth), context.sceneView);
		const float3 endWS   = NDCToWorldSpace(float3(endUV * 2.f - 1.f, endDepth), context.sceneView);

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

#endif // SCREEN_SPACE_TRACER_HLSLI
