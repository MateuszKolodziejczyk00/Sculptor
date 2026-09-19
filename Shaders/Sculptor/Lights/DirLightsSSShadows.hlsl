#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]

[[shader_params(ScreenSpaceShadowsConstants, PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS)]]


#define DEBUG_SCREEN_SPACE_TRACER DEBUG_RAY


#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Wave.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/ScreenSpaceTracer.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void TraceSSShadowsCS(CS_INPUT input)
{
	const uint traceCommandIndex = input.globalID.x;

	if(traceCommandIndex >= PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->inCommandsNum.Load(0))
	{
		return;
	}

	if (traceCommandIndex == 0u)
	{
		PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->outRTTraceIndirectArgs.Store(1u, 1u);
		PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->outRTTraceIndirectArgs.Store(2u, 1u);
	}

	const EncodedRayTraceCommand encodedTraceCommand = PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->inCommands.Load(traceCommandIndex);
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 coords = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->depth.Load(uint3(coords, 0)).r;

	if(depth == 0.f)
	{
		return;
	}

	const float3 N = OctahedronDecodeNormal(PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->normal.Load(uint3(coords, 0)));
	const float3 L = -PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->lightDirection;

	if (dot(N, L) < 0.f)
	{
		PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->outShadows.Store(coords, 0.f);
		return;
	}

	const float2 startUV = (coords + 0.5f) * PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->pixelSize;

	const float noise = frac(PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->blueNoise256.Load(uint3(coords & 255u, 0)).r + PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->frameIdx * SPT_GOLDEN_RATIO);

	const SSTraceResultExtended traceResult = TraceScreenSpaceRay(PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->ssTracerData, VIEW->sceneView, startUV, depth, L, PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->traceDistance, noise);

	PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->outShadows.Store(coords, traceResult.isHit ? 0.f : 1.f);

	if (traceResult.isHit)
	{
		PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->outShadows.Store(coords, 0.f);
	}
	else
	{
		const uint2 rtTracesBallot = WaveActiveBallot(true).xy;
		const uint rtTracesNum = countbits(rtTracesBallot.x) + countbits(rtTracesBallot.y);

		uint rtTraceCommandsOffset;
		if(WaveIsFirstLane())
		{
			rtTraceCommandsOffset = PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->outRTTraceIndirectArgs.AtomicAdd(0, rtTracesNum);
		}

		const uint outputCommandIdx = WaveReadLaneFirst(rtTraceCommandsOffset) + GetCompactedIndex(rtTracesBallot, WaveGetLaneIndex());

		PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->outRTCommands.Store(outputCommandIdx, encodedTraceCommand);

		const float rayUnoccludedDist = traceResult.unoccludedDistance;
		const float continuationDist = rayUnoccludedDist / PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->traceDistance;
		PARAMS_SCREEN_SPACE_SHADOWS_CONSTANTS->outContinuationDists.Store(outputCommandIdx, uint16_t(continuationDist * 255u));
	}
}
