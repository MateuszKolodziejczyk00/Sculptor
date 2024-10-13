#include "SculptorShader.hlsli"

[[descriptor_set(SRResamplingFinalVisibilityTestDS, 2)]]
[[descriptor_set(RenderViewDS, 3)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/RTVisibilityCommon.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/MortonCode.hlsli"
#include "SpecularReflections/SRReservoir.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"

#ifdef SPT_RT_GENERATION_SHADER
#ifndef FORCE_FULL_RATE
#error "FORCE_FULL_RATE must be defined"
#endif // FORCE_FULL_RATE
#endif // SPT_RT_GENERATION_SHADER


uint CreateInvalidationMask(in uint variableRate)
{
	uint invalidationMask = 1u;
	if(variableRate & (SPT_VARIABLE_RATE_2X | SPT_VARIABLE_RATE_4X))
	{
		invalidationMask |= 2u;
	}
	if(variableRate & (SPT_VARIABLE_RATE_2Y | SPT_VARIABLE_RATE_4Y))
	{
		invalidationMask |= 4u;
	}
	if(variableRate >= SPT_VARIABLE_RATE_2X2)
	{
		invalidationMask |= 8u;
	}

	if(variableRate & SPT_VARIABLE_RATE_4X)
	{
		invalidationMask |= (0xF << 4);
	}
	if(variableRate & SPT_VARIABLE_RATE_4Y)
	{
		invalidationMask |= (0xF << 8);
	}

	if(variableRate >= SPT_VARIABLE_RATE_4X4)
	{
		invalidationMask |= (0xF << 12);
	}

	return invalidationMask;
}



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
		RTVisibilityPayload payload = { false };

		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

		const float bias = 0.07f;

		const uint reservoirIdx =  GetScreenReservoirIdx(pixel.xy, u_resamplingConstants.reservoirsResolution);
		const SRReservoir reservoir = UnpackReservoir(u_inOutReservoirsBuffer[reservoirIdx]);
		if (!reservoir.HasFlag(SR_RESERVOIR_FLAGS_RECENT))
		{
			const float distance = length(reservoir.hitLocation - worldLocation);

			if(distance > 2.5f * bias)
			{
				const float3 rayDirection = (reservoir.hitLocation - worldLocation) / distance;

				RayDesc rayDesc;
				rayDesc.TMin        = bias;
				rayDesc.TMax        = distance - bias;
				rayDesc.Origin      = worldLocation;
				rayDesc.Direction   = rayDirection;

				TraceRay(u_sceneTLAS,
						 RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
						 0xFF,
						 0,
						 1,
						 0,
						 rayDesc,
						 payload);
			}

			if(!payload.isVisible)
			{
				const uint2 invalidationCoords = traceCommand.blockCoords / uint2(8u, 4u);
				const uint2 invalidationTileOffset = traceCommand.blockCoords & uint2(7u, 3u);

				const uint invalidationMask = CreateInvalidationMask(traceCommand.variableRateMask);

				const uint invalidationMaskOffset = EncodeMorton2(invalidationTileOffset);

				InterlockedOr(u_rwInvalidationMask[invalidationCoords], (invalidationMask << invalidationMaskOffset));
			}
		}
	}
}
