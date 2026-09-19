#include "SculptorShader.hlsli"

#define RT_MATERIAL_TRACING

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(GPURenderView, VIEW)]]
[[shader_params(SpecularReflectionsTraceConstants, PARAMS_SPECULAR_REFLECTIONS_TRACE)]]

#include "SpecularReflections/RTGICommon.hlsli"
#include "SpecularReflections/RTGBuffer.hlsli"
#include "SpecularReflections/RTGITracing.hlsli"

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
		PARAMS_SPECULAR_REFLECTIONS_TRACE->shadingIndirectArgs[0].hitDispatchSize.yz    = 1u;
		PARAMS_SPECULAR_REFLECTIONS_TRACE->shadingIndirectArgs[0].missDispatchGroups.yz = 1u;
	}

	const EncodedRayTraceCommand encodedTraceCommand = PARAMS_SPECULAR_REFLECTIONS_TRACE->traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 pixel = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = PARAMS_SPECULAR_REFLECTIONS_TRACE->gpuGBuffer.depth.Load(uint3(pixel, 0));
	if (depth > 0.f)
	{
		const float2 uv = (pixel + 0.5f) * PARAMS_SPECULAR_REFLECTIONS_TRACE->invResolution;
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		float3 worldLocation = NDCToWorldSpace(ndc, VIEW->sceneView);

		const uint encodedRayDirection = PARAMS_SPECULAR_REFLECTIONS_TRACE->rayDirections[traceCommandIndex];

		const float3 rayDirection = OctahedronDecodeNormal(UnpackHalf2x16Norm(encodedRayDirection));

		RngState rng = RngState::Create(pixel, 123u);

		RayHitResult hitResult;
		bool isRayFinished = false;

		const float traceLength = PARAMS_SPECULAR_REFLECTIONS_TRACE->ssrTraceLength;
		const SSTraceResultExtended ssResult = TraceScreenSpaceRay(PARAMS_SPECULAR_REFLECTIONS_TRACE->ssrTracer, VIEW->sceneView, uv, depth, rayDirection, traceLength, rng.Next());

		float3 traceOrigin = worldLocation;

		if (ssResult.isHit)
		{
			const GBufferInterface gbuffer = GBufferInterface(PARAMS_SPECULAR_REFLECTIONS_TRACE->gpuGBuffer);

			const SurfaceInfo hitSurfaceInfo = gbuffer.GetSurfaceInfo(ssResult.hitUV);
			hitResult.normal      = hitSurfaceInfo.normal;
			hitResult.roughness   = hitSurfaceInfo.roughness;
			hitResult.baseColor   = hitSurfaceInfo.baseColorMetallic.rgb;
			hitResult.metallic    = hitSurfaceInfo.baseColorMetallic.w;
			hitResult.emissive    = 0.f;
			hitResult.hitType     = RTGBUFFER_HIT_TYPE_VALID_HIT;
			hitResult.hitDistance = traceLength * ssResult.hitT;

			isRayFinished = true;
		}
		else
		{
			traceOrigin += rayDirection * ssResult.unoccludedDistance * 0.99f;
		}

		if (!isRayFinished)
		{
			hitResult = RTGITraceRay(traceOrigin , rayDirection);

			if (hitResult.hitType != RTGBUFFER_HIT_TYPE_NO_HIT)
			{
				hitResult.hitDistance += ssResult.unoccludedDistance * 0.99f;
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
				validHitsOffset = PARAMS_SPECULAR_REFLECTIONS_TRACE->raysShadingCounts.Cast<uint>().AtomicAdd(0u, validHitCount);

				PARAMS_SPECULAR_REFLECTIONS_TRACE->shadingIndirectArgs.Cast<uint>().AtomicMax(0u, validHitsOffset + validHitCount);
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
				missOffset = PARAMS_SPECULAR_REFLECTIONS_TRACE->raysShadingCounts.Cast<uint>().AtomicAdd(1u, missCount);

				PARAMS_SPECULAR_REFLECTIONS_TRACE->shadingIndirectArgs.Cast<uint>().AtomicMax(4u, (missOffset + missCount + 63) / 64);
			}
			const uint compactedIdx = GetCompactedIndex(missMask, WaveGetLaneIndex());
			const uint missIndex = PARAMS_SPECULAR_REFLECTIONS_TRACE->rayCommandsBufferSize - WaveReadLaneFirst(missOffset) - missCount + compactedIdx;
			hitResultIdx = missIndex;
		}

		if(hitResultIdx != IDX_NONE_32)
		{
			PARAMS_SPECULAR_REFLECTIONS_TRACE->sortedRays[hitResultIdx] = traceCommandIndex;
			PARAMS_SPECULAR_REFLECTIONS_TRACE->hitMaterialInfos[traceCommandIndex] = PackRTGBuffer(hitResult);
		}
	}
}
