#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS)]]

[[shader_params(ScreenSpaceShadowsConstants, u_constants)]]


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

	if(traceCommandIndex >= u_constants.inCommandsNum.Load(0))
	{
		return;
	}

	if (traceCommandIndex == 0u)
	{
		u_constants.outRTTraceIndirectArgs.Store(1u, 1u);
		u_constants.outRTTraceIndirectArgs.Store(2u, 1u);
	}

	const EncodedRayTraceCommand encodedTraceCommand = u_constants.inCommands.Load(traceCommandIndex);
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 coords = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = u_constants.depth.Load(uint3(coords, 0)).r;

	if(depth == 0.f)
	{
		return;
	}

	const float3 N = OctahedronDecodeNormal(u_constants.normal.Load(uint3(coords, 0)));
	const float3 L = -u_constants.lightDirection;

	if (dot(N, L) < 0.f)
	{
		u_constants.outShadows.Store(coords, 0.f);
		return;
	}

	const float2 startUV = (coords + 0.5f) * u_constants.pixelSize;

	const float noise = frac(u_constants.blueNoise256.Load(uint3(coords & 255u, 0)).r + u_constants.frameIdx * SPT_GOLDEN_RATIO);

	const SSTraceResultExtended traceResult = TraceScreenSpaceRay(u_constants.ssTracerData, u_sceneView, startUV, depth, L, u_constants.traceDistance, noise);

	u_constants.outShadows.Store(coords, traceResult.isHit ? 0.f : 1.f);

	if (traceResult.isHit)
	{
		u_constants.outShadows.Store(coords, 0.f);
	}
	else
	{
		const uint2 rtTracesBallot = WaveActiveBallot(true).xy;
		const uint rtTracesNum = countbits(rtTracesBallot.x) + countbits(rtTracesBallot.y);

		uint rtTraceCommandsOffset;
		if(WaveIsFirstLane())
		{
			rtTraceCommandsOffset = u_constants.outRTTraceIndirectArgs.AtomicAdd(0, rtTracesNum);
		}

		const uint outputCommandIdx = WaveReadLaneFirst(rtTraceCommandsOffset) + GetCompactedIndex(rtTracesBallot, WaveGetLaneIndex());

		u_constants.outRTCommands.Store(outputCommandIdx, encodedTraceCommand);

		const float rayUnoccludedDist = traceResult.unoccludedDistance;
		const float continuationDist = rayUnoccludedDist / u_constants.traceDistance;
		u_constants.outContinuationDists.Store(outputCommandIdx, uint16_t(continuationDist * 255u));
	}
}
