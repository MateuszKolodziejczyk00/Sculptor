#include "SculptorShader.hlsli"

#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/SpecularReflectionsTracingCommon.hlsli"
#include "SpecularReflections/RTGBuffer.hlsli"

#include "Utils/SceneViewUtils.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


RayHitResult TraceReflectionRay(in float3 rayOrigin, in float3 rayDirection)
{
	const float maxHitDistance = 200.f;

	RayDesc rayDesc;
	rayDesc.TMin      = 0.004f;
	rayDesc.TMax      = maxHitDistance;
	rayDesc.Origin    = rayOrigin;
	rayDesc.Direction = rayDirection;

	SpecularReflectionsRayPayload payload;

	TraceRay(u_sceneTLAS,
			 0,
			 0xFF,
			 0,
			 1,
			 0,
			 rayDesc,
			 payload);

	const bool isBackface = payload.distance < 0.f;
	const bool isValidHit = !isBackface && payload.distance < maxHitDistance;

	RayHitResult hitResult = RayHitResult::CreateEmpty();

	if (isValidHit)
	{
		const float4 baseColorMetallic = UnpackFloat4x8(payload.baseColorMetallic);

		hitResult.normal      = normalize(payload.normal);
		hitResult.roughness   = payload.roughness;
		hitResult.baseColor   = baseColorMetallic.rgb;
		hitResult.metallic    = baseColorMetallic.w;
		hitResult.hitType     = RTGBUFFER_HIT_TYPE_VALID_HIT;
		hitResult.hitDistance = payload.distance;
	}
	else if (!isBackface)
	{
		hitResult.hitType = RTGBUFFER_HIT_TYPE_NO_HIT;
	}
	else
	{
		hitResult.hitType = RTGBUFFER_HIT_TYPE_BACKFACE;
	}

	return hitResult;
}


[shader("raygeneration")]
void GenerateSpecularReflectionsRaysRTG()
{
	const uint traceCommandIndex = DispatchRaysIndex().x;

	const EncodedRayTraceCommand encodedTraceCommand = u_traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 pixel = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = u_depthTexture.Load(uint3(pixel, 0));
	if (depth > 0.f)
	{
		const float2 uv = (pixel + 0.5f) * u_constants.invResolution;
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

		const uint encodedRayDirection = u_rayDirections[traceCommandIndex];

		const float3 rayDirection = OctahedronDecodeNormal(UnpackHalf2x16Norm(encodedRayDirection));

		const RayHitResult hitResult = TraceReflectionRay(worldLocation, rayDirection);

		u_hitMaterialInfos[traceCommandIndex] = PackRTGBuffer(hitResult);
	}
}

[shader("miss")]
void SpecularReflectionsRTM(inout SpecularReflectionsRayPayload payload)
{
	payload.distance = 999999.f;
}
