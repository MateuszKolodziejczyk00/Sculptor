#include "SculptorShader.hlsli"

#include "SpecularReflections/RTGITracingDescriptors.hlsli"

[[descriptor_set(SpecularReflectionsTraceDS, 6)]]

#include "SpecularReflections/RTGICommon.hlsli"
#include "SpecularReflections/RTGBuffer.hlsli"
#include "SpecularReflections/RTGITracing.hlsli"

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Wave.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


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

		const RayHitResult hitResult = RTGITraceRay(u_sceneTLAS, worldLocation, rayDirection);

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
