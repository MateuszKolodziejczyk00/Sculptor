#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(SRResamplingFinalVisibilityTestDS)]]
[[descriptor_set(RenderViewDS)]]

#include "RayTracing/RayTracingHelpers.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/MortonCode.hlsli"
#include "SpecularReflections/SRReservoir.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"

#include "Utils/ScreenSpaceTracer.hlsli"
#include "Utils/Random.hlsli"

#ifdef SPT_RT_GENERATION_SHADER
#ifndef FORCE_FULL_RATE
#error "FORCE_FULL_RATE must be defined"
#endif // FORCE_FULL_RATE
#endif // SPT_RT_GENERATION_SHADER


[shader("raygeneration")]
void ResamplingFinalVisibilityTestRTG()
{
#if FORCE_FULL_RATE
	const uint2 coords = DispatchRaysIndex().xy;

	RayTraceCommand traceCommand;
	traceCommand.blockCoords      = coords;
	traceCommand.localOffset      = 0u;
	traceCommand.variableRateMask = SPT_VARIABLE_RATE_1X1;
#else
	const uint traceCommandIndex = DispatchRaysIndex().x;

	const EncodedRayTraceCommand encodedTraceCommand = u_traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);
#endif // FORCE_FULL_RATE

	const uint3 pixel = uint3(min(traceCommand.blockCoords + traceCommand.localOffset, u_resamplingConstants.resolution - 1u), 0);

	const float2 uv = (pixel.xy + 0.5f) * u_resamplingConstants.pixelSize;
	const float depth = u_depthTexture.Load(pixel);

	if(depth > 0.f)
	{
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

		const float bias = 0.005f;

		const uint reservoirIdx =  GetScreenReservoirIdx(pixel.xy, u_resamplingConstants.reservoirsResolution);
		const SRReservoir reservoir = UnpackReservoir(u_inOutReservoirsBuffer[reservoirIdx]);
		if (!reservoir.HasFlag(SR_RESERVOIR_FLAGS_VALIDATED))
		{
			bool isVisible = false;

			const float distance = length(reservoir.hitLocation - worldLocation);

			if(distance > 2.5f * bias)
			{
				const float3 rayDirection = (reservoir.hitLocation - worldLocation) / distance;

				const RngState rng = RngState::Create(pixel.xy, 123u);

				const float traceDist = min(u_resamplingConstants.ssrTraceLength, distance - bias);
				const SSTraceResultExtended ssResult = TraceScreenSpaceRay(u_resamplingConstants.ssTracer, u_sceneView, uv, depth, rayDirection, traceDist, rng.Next());
				if (ssResult.isHit)
				{
					isVisible = false;
				}
				else
				{
					RayDesc rayDesc;
					rayDesc.TMin        = ssResult.unoccludedDistance * 0.99f;
					rayDesc.TMax        = distance - bias;
					rayDesc.Origin      = worldLocation;
					rayDesc.Direction   = rayDirection;

					isVisible = RTScene().VisibilityTest(rayDesc);
				}
			}

			if(!isVisible)
			{
				SRPackedReservoir initialReservoir = u_initialReservoirsBuffer[reservoirIdx];
				uint packedProps = initialReservoir.MAndProps;
				packedProps = ModifyPackedSpatialResamplingRangeID(packedProps, -3);
				initialReservoir.MAndProps = packedProps;

				u_inOutReservoirsBuffer[reservoirIdx] = initialReservoir;
			}
		}
	}
}
