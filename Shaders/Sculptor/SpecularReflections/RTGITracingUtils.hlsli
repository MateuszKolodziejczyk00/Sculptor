#ifndef RTGI_TRACING_UTILS_HLSLI
#define RTGI_TRACING_UTILS_HLSLI

#include "RayTracing/RayTracingHelpers.hlsli"

#include "Shading/Shading.hlsli"
#include "Utils/Random.hlsli"

#include "SpecularReflections/RTGBuffer.hlsli"


#ifdef PARAM_RenderSceneConstants
RayHitResult RTGITraceRay(in float3 rayOrigin, in float3 rayDirection)
{
	const float maxHitDistance = 800.f;

	RayDesc rayDesc;
	rayDesc.TMin      = 0.f;
	rayDesc.TMax      = maxHitDistance;
	rayDesc.Origin    = rayOrigin;
	rayDesc.Direction = rayDirection;

	const RayPayloadData traceResult = RTScene().TraceMaterialRay(rayDesc);

	RayHitResult hitResult = RayHitResult::CreateEmpty();

	if (traceResult.visibility.isValidHit)
	{
		hitResult.SetHitMaterial(traceResult.material);
	}
	else if (traceResult.visibility.isMiss)
	{
		hitResult.hitType = RTGBUFFER_HIT_TYPE_NO_HIT;
	}
	else
	{
		hitResult.hitType = RTGBUFFER_HIT_TYPE_BACKFACE;
	}

	return hitResult;
}
#endif // PARAM_RenderSceneConstants

#endif // RTGI_TRACING_UTILS_HLSLI
