#include "SculptorShader.hlsli"

#define RT_MATERIAL_TRACING

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(GPURenderView, VIEW)]]
[[shader_params(SpecularReflectionsTraceConstants, CONSTS)]]

#include "SpecularReflections/RTGBuffer.hlsli"
#include "SpecularReflections/RTGITracingUtils.hlsli"

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Wave.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"

#include "Utils/ScreenSpaceTracer.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"


[shader("raygeneration")]
void GenerateRTGIRaysRTG()
{
	const uint traceCommandIndex = DispatchRaysIndex().x;

	if(traceCommandIndex == 0u)
	{
		CONSTS->shadingIndirectArgs[0].hitDispatchSize.yz    = 1u;
		CONSTS->shadingIndirectArgs[0].missDispatchGroups.yz = 1u;
	}

	const EncodedRayTraceCommand encodedTraceCommand = CONSTS->traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 pixel = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = CONSTS->gpuGBuffer.depth.Load(uint3(pixel, 0));
	if (depth > 0.f)
	{
		const float2 uv = (pixel + 0.5f) * CONSTS->invResolution;
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		float3 worldLocation = NDCToWorldSpace(ndc, VIEW->sceneView);

		const uint encodedRayDirection = CONSTS->rayDirections[traceCommandIndex];

		const float3 rayDirection = OctahedronDecodeNormal(UnpackHalf2x16Norm(encodedRayDirection));

		RngState rng = RngState::Create(pixel, CONSTS->frameIdx);

		RayHitResult hitResult;
		bool isRayFinished = false;

		const float traceLength = CONSTS->ssrTraceLength;
		const SSTraceResult ssResult = TraceScreenSpaceRay<false, 2u>(CONSTS->ssrTracer, VIEW->sceneView, uv, depth, rayDirection, traceLength, rng.Next());

		float3 traceOrigin = worldLocation;

		if (ssResult.isHit)
		{
			const GBufferInterface gbuffer = GBufferInterface(CONSTS->gpuGBuffer);

			const SurfaceInfo hitSurfaceInfo = gbuffer.GetSurfaceInfo(ssResult.hitUV * CONSTS->resolution);
			hitResult.normal            = hitSurfaceInfo.normal;
			hitResult.roughness         = hitSurfaceInfo.roughness;
			hitResult.baseColor         = hitSurfaceInfo.baseColorMetallic.rgb;
			hitResult.metallic          = hitSurfaceInfo.baseColorMetallic.a;
			hitResult.emissive          = 0.f;
			hitResult.hitType           = RTGBUFFER_HIT_TYPE_VALID_HIT;
			hitResult.hitDistance       = ssResult.hitDist;

			isRayFinished = true;
		}
		else
		{
			traceOrigin += rayDirection * max(ssResult.unoccludedDist * 0.99f, 0.02f);
		}

		if (!isRayFinished)
		{
			hitResult = RTGITraceRay(traceOrigin , rayDirection);

			if (hitResult.hitType != RTGBUFFER_HIT_TYPE_NO_HIT)
			{
				hitResult.hitDistance += ssResult.unoccludedDist * 0.99f;
			}
		}

		uint hitResultIdx = IDX_NONE_32;

		const bool isValidHit = hitResult.hitType == RTGBUFFER_HIT_TYPE_VALID_HIT;
		if(isValidHit)
		{
			const uint validHitMask = WaveActiveBallot(isValidHit).x;
			const uint validHitCount = countbits(validHitMask);
			uint validHitsOffset = 0;
			if(WaveIsFirstLane())
			{
				validHitsOffset = CONSTS->raysShadingCounts.Cast<uint>().AtomicAdd(0u, validHitCount);

				CONSTS->shadingIndirectArgs.Cast<uint>().AtomicMax(0u, validHitsOffset + validHitCount);
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
				missOffset = CONSTS->raysShadingCounts.Cast<uint>().AtomicAdd(1u, missCount);

				CONSTS->shadingIndirectArgs.Cast<uint>().AtomicMax(4u, (missOffset + missCount + 63) / 64);
			}
			const uint compactedIdx = GetCompactedIndex(missMask, WaveGetLaneIndex());
			const uint missIndex = CONSTS->rayCommandsBufferSize - WaveReadLaneFirst(missOffset) - missCount + compactedIdx;
			hitResultIdx = missIndex;
		}

		if(hitResultIdx != IDX_NONE_32)
		{
			CONSTS->sortedRays[hitResultIdx] = traceCommandIndex;
			CONSTS->hitMaterialInfos[traceCommandIndex] = PackRTGBuffer(hitResult);
		}
	}
}
