#include "SculptorShader.hlsli"

[[descriptor_set(RTShadingDS, 0)]]
[[descriptor_set(RenderViewDS, 1)]]
[[descriptor_set(CloudscapeProbesDS, 2)]]

#include "Utils/SceneViewUtils.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Atmosphere/VolumetricClouds/Cloudscape.hlsli"
#include "SpecularReflections/SRReservoir.hlsli"
#include "SpecularReflections/RTGBuffer.hlsli"
#include "SpecularReflections/RTReflectionsShadingCommon.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


struct CS_INPUT
{
	uint3 globalID : SV_DispatchThreadID;
};


[numthreads(64, 1, 1)]
void MissRaysShadingCS(CS_INPUT input)
{
	const uint missIdx = input.globalID.x;

	const uint missRaysNum = u_tracesNum[0].missRaysNum;

	if(missIdx >= missRaysNum)
	{
		return;
	}

	const uint missRaysOffset = u_constants.rayCommandsBufferSize - missRaysNum;
	const uint rayIdx = missRaysOffset + missIdx;

	const uint traceCommandIndex = u_sortedTraces[rayIdx];

	const EncodedRayTraceCommand encodedTraceCommand = u_traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 pixel = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = u_depthTexture.Load(uint3(pixel, 0));
	if(depth > 0.f)
	{
		const float2 uv = (pixel + 0.5f) * u_constants.invResolution;
		const float3 ndc = float3(uv * 2.f - 1.f, depth);

		const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);
		
		const RayHitResult hitResult = UnpackRTGBuffer(u_hitMaterialInfos[traceCommandIndex]);

		if(hitResult.hitType == RTGBUFFER_HIT_TYPE_NO_HIT)
		{
			const uint encodedRayDirection = u_rayDirections[traceCommandIndex];
			const float3 rayDirection = OctahedronDecodeNormal(UnpackHalf2x16Norm(encodedRayDirection));

			const float3 reservoirHitLocation = worldLocation + rayDirection * 2000.f;

			const float3 locationInAtmoshpere = GetLocationInAtmosphere(u_atmosphereParams, worldLocation);
			float3 luminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, locationInAtmoshpere, rayDirection);

			const CloudscapeSample cloudscapeSample = SampleHighResCloudscape(rayDirection);
			luminance = cloudscapeSample.inScattering + luminance * cloudscapeSample.transmittance;

			const float fogTransmittance = EvaluateHeightBasedTransmittanceForSegment(u_constants.heightFog, worldLocation, reservoirHitLocation);
			luminance *= fogTransmittance;

			const GeneratedRayPDF rayPdf = LoadGeneratedRayPDF(u_rayPdfs, traceCommandIndex);

			SRReservoir reservoir = SRReservoir::Create(reservoirHitLocation, 0.f, luminance, rayPdf.pdf);

			reservoir.AddFlag(SR_RESERVOIR_FLAGS_MISS);
			reservoir.AddFlag(SR_RESERVOIR_FLAGS_VALIDATED);

			if(rayPdf.isSpecularTrace)
			{
				reservoir.AddFlag(SR_RESERVOIR_FLAGS_SPECULAR_TRACE);
			}

			reservoir.luminance = LuminanceToExposedLuminance(reservoir.luminance);

			WriteReservoirToScreenBuffer(u_reservoirsBuffer, u_constants.reservoirsResolution, reservoir, traceCommand);
		}
	}
}
