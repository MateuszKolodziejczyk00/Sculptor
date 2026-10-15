#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]

[[shader_params(ScreenSpaceShadowsConstants, CONSTS)]]


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

	if(traceCommandIndex >= CONSTS->inCommandsNum.Load(0))
	{
		return;
	}

	if (traceCommandIndex == 0u)
	{
		CONSTS->outRTTraceIndirectArgs.Store(1u, 1u);
		CONSTS->outRTTraceIndirectArgs.Store(2u, 1u);
	}

	const EncodedRayTraceCommand encodedTraceCommand = CONSTS->inCommands.Load(traceCommandIndex);
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 coords = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = CONSTS->depth.Load(uint3(coords, 0)).r;

	if(depth == 0.f)
	{
		return;
	}

	const float3 N = OctahedronDecodeNormal(CONSTS->normal.Load(uint3(coords, 0)));
	const float3 L = -CONSTS->lightDirection;

	if (dot(N, L) < 0.f)
	{
		CONSTS->outShadows.Store(coords, 0.f);
		return;
	}

	const float2 startUV = (coords + 0.5f) * CONSTS->pixelSize;

	const float noise = frac(GLOBALS->blueNoise256.Load(uint3(coords & 255u, 0)).r + CONSTS->frameIdx * SPT_GOLDEN_RATIO);

	const SSTraceResult traceResult = TraceScreenSpaceRay<true>(CONSTS->ssTracerData, VIEW->sceneView, startUV, depth, L, CONSTS->traceDistance, noise);

	CONSTS->outShadows.Store(coords, traceResult.isHit ? 0.f : 1.f);

	if (traceResult.isHit)
	{
		CONSTS->outShadows.Store(coords, 0.f);
	}
	else
	{
		const uint2 rtTracesBallot = WaveActiveBallot(true).xy;
		const uint rtTracesNum = countbits(rtTracesBallot.x) + countbits(rtTracesBallot.y);

		uint rtTraceCommandsOffset;
		if(WaveIsFirstLane())
		{
			rtTraceCommandsOffset = CONSTS->outRTTraceIndirectArgs.AtomicAdd(0, rtTracesNum);
		}

		const uint outputCommandIdx = WaveReadLaneFirst(rtTraceCommandsOffset) + GetCompactedIndex(rtTracesBallot, WaveGetLaneIndex());

		CONSTS->outRTCommands.Store(outputCommandIdx, encodedTraceCommand);

		const float continuationDist = traceResult.unoccludedDist / CONSTS->traceDistance;

		CONSTS->outContinuationDists.Store(outputCommandIdx, uint16_t(continuationDist * 255u));
	}
}
