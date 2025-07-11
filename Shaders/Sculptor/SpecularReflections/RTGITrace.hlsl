#include "SculptorShader.hlsli"

#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/RTGITracingCommon.hlsli"
#include "SpecularReflections/RTGBuffer.hlsli"

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Wave.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


RayHitResult TraceReflectionRay(in float3 rayOrigin, in float3 rayDirection)
{
	const float maxHitDistance = 200.f;

	const float bias = 0.05f;

	const float3 biasedRayOrigin = rayOrigin;

	RayDesc rayDesc;
	rayDesc.TMin      = 0.01f;
	rayDesc.TMax      = maxHitDistance;
	rayDesc.Origin    = biasedRayOrigin;
	rayDesc.Direction = rayDirection;

	RTGIRayPayload payload;

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
		hitResult.emissive    = payload.emissive;
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
void GenerateRTGIRaysRTG()
{
	const uint traceCommandIndex = DispatchRaysIndex().x;

	if(traceCommandIndex == 0u)
	{
		u_shadingIndirectArgs[0].hitDispatchSize.yz    = 1u;
		u_shadingIndirectArgs[0].missDispatchGroups.yz = 1u;
	}

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

		uint hitResultIdx = IDX_NONE_32;

		const bool isValidHit = hitResult.hitType == RTGBUFFER_HIT_TYPE_VALID_HIT;
		if(isValidHit)
		{
			const uint validHitMask = WaveActiveBallot(isValidHit).x;
			const uint validHitCount = countbits(validHitMask);
			uint validHitsOffset = 0;
			if(WaveIsFirstLane())
			{
				InterlockedAdd(u_raysShadingCounts[0].hitRaysNum, validHitCount, OUT validHitsOffset);

				InterlockedMax(u_shadingIndirectArgs[0].hitDispatchSize.x, validHitsOffset + validHitCount);
			}
			const uint validHitIndex = WaveReadLaneFirst(validHitsOffset) + GetCompactedIndex(validHitMask, WaveGetLaneIndex());
			hitResultIdx = validHitIndex;
		}

		const bool isMiss = hitResult.hitType == RTGBUFFER_HIT_TYPE_NO_HIT;
		if (isMiss)
		{
			const uint missMask = WaveActiveBallot(isMiss).x;
			const uint missCount = countbits(missMask);
			uint missOffset = 0;
			if (WaveIsFirstLane())
			{
				InterlockedAdd(u_raysShadingCounts[0].missRaysNum, missCount, OUT missOffset);

				InterlockedMax(u_shadingIndirectArgs[0].missDispatchGroups.x, (missOffset + missCount + 63) / 64);
			}
			const uint compactedIdx = GetCompactedIndex(missMask, WaveGetLaneIndex());
			const uint missIndex = u_constants.rayCommandsBufferSize - WaveReadLaneFirst(missOffset) - missCount + compactedIdx;
			hitResultIdx = missIndex;
		}

		if(hitResultIdx != IDX_NONE_32)
		{
			u_sortedRays[hitResultIdx] = traceCommandIndex;
			u_hitMaterialInfos[traceCommandIndex] = PackRTGBuffer(hitResult);
		}
	}
}


[shader("miss")]
void RTGIRTM(inout RTGIRayPayload payload)
{
	payload.distance = 999999.f;
}
