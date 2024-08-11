#include "SculptorShader.hlsli"

[[descriptor_set(SRResamplingFinalVisibilityTestDS, 2)]]
[[descriptor_set(RenderViewDS, 3)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/RTVisibilityCommon.hlsli"
#include "Utils/Packing.hlsli"
#include "SpecularReflections/SRReservoir.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


[shader("raygeneration")]
void ResamplingFinalVisibilityTestRTG()
{
	const uint traceCommandIndex = DispatchRaysIndex().x;

	const EncodedRayTraceCommand encodedTraceCommand = u_traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint3 pixel = uint3(traceCommand.blockCoords + traceCommand.localOffset, 0);

	const float2 uv = (pixel.xy + 0.5f) * u_resamplingConstants.pixelSize;
	const float depth = u_depthTexture.Load(pixel);

	if(depth > 0.f)
	{
		RTVisibilityPayload payload = { false };

		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

		const float3 normal = OctahedronDecodeNormal(u_normalsTexture.Load(pixel));

		const float bias = 0.01f;

		const float3 biasedWorldLocation = worldLocation + normal * bias;

		const uint reservoirIdx =  GetScreenReservoirIdx(pixel.xy, u_resamplingConstants.reservoirsResolution);
		const SRReservoir reservoir = UnpackReservoir(u_inOutReservoirsBuffer[reservoirIdx]);
		if (!reservoir.HasFlag(SR_RESERVOIR_FLAGS_RECENT))
		{
			const float3 biasedHitLocation = reservoir.hitLocation + reservoir.hitNormal * bias;

			const float distance = length(biasedHitLocation - biasedWorldLocation);

			const float3 rayDirection = (biasedHitLocation - biasedWorldLocation) / distance;

			RayDesc rayDesc;
			rayDesc.TMin        = 0.0f;
			rayDesc.TMax        = distance;
			rayDesc.Origin      = biasedWorldLocation;
			rayDesc.Direction   = rayDirection;

			TraceRay(u_sceneTLAS,
					 RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
					 0xFF,
					 0,
					 1,
					 0,
					 rayDesc,
					 payload);

			if(!payload.isVisible)
			{
				u_inOutReservoirsBuffer[reservoirIdx] = u_initialReservoirsBuffer[reservoirIdx];
			}
		}
	}
}
