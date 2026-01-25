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
		return;
	}

	const float2 startUV = (coords + 0.5f) * u_constants.pixelSize;
	const float startDepth = depth;

	const float3 startNDC = float3(startUV * 2.f - 1.f, startDepth);
	const float3 startWS = NDCToWorldSpace(startNDC, u_sceneView);

	const float3 endWS = startWS + L * u_constants.traceDistance;
	const float3 endNDC = WorldSpaceToNDC(endWS, u_sceneView);

	const float2 endUV = endNDC.xy * 0.5f + 0.5f;
	const float endDepth = endNDC.z;

	const float noise = frac(u_constants.blueNoise256.Load(uint3(coords & 255u, 0)).r + u_constants.frameIdx * SPT_GOLDEN_RATIO);

	ScreenSpaceTracerContext tracerContext;
	tracerContext.sceneView      = u_sceneView;
	tracerContext.linearDepth    = u_constants.linearDepth.GetResource();
	tracerContext.resolution     = u_constants.resolution;
	tracerContext.stepsNum       = u_constants.stepsNum;

	const SSTraceResult traceResult = TraceSSShadowRay(tracerContext, startUV, startDepth, endUV, endDepth, noise);

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
	}
}
