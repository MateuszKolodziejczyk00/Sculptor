#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(CloudscapeProbesParams, CLOUDSCAPE)]]
[[shader_params(RTShadingConstants, CONSTS)]]

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

	const uint missRaysNum = CONSTS->tracesNum[0].missRaysNum;

	if(missIdx >= missRaysNum)
	{
		return;
	}

	const uint missRaysOffset = CONSTS->rayCommandsBufferSize - missRaysNum;
	const uint rayIdx = missRaysOffset + missIdx;

	const uint traceCommandIndex = CONSTS->sortedTraces[rayIdx];

	const EncodedRayTraceCommand encodedTraceCommand = CONSTS->traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 pixel = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = CONSTS->depthTexture.Load(uint3(pixel, 0));
	if(depth > 0.f)
	{
		const float2 uv = (pixel + 0.5f) * CONSTS->invResolution;
		const float3 ndc = float3(uv * 2.f - 1.f, depth);

		const float3 worldLocation = NDCToWorldSpace(ndc, VIEW->sceneView);
		
		const RayHitResult hitResult = UnpackRTGBuffer(CONSTS->hitMaterialInfos[traceCommandIndex]);

		if(hitResult.hitType == RTGBUFFER_HIT_TYPE_NO_HIT)
		{
			const uint encodedRayDirection = CONSTS->rayDirections[traceCommandIndex];
			const float3 rayDirection = OctahedronDecodeNormal(UnpackHalf2x16Norm(encodedRayDirection));

			const float3 reservoirHitLocation = worldLocation + rayDirection * 2000.f;

			const float3 locationInAtmoshpere = GetLocationInAtmosphere(*CONSTS->atmosphereParams, worldLocation);
			float3 luminance = GetLuminanceFromSkyViewLUT(*CONSTS->atmosphereParams, CONSTS->skyViewLUT, BindlessSamplers::LinearClampEdge(), locationInAtmoshpere, rayDirection);

			const CloudscapeSample cloudscapeSample = SampleHighResCloudscape(worldLocation, rayDirection);
			luminance = cloudscapeSample.inScattering + luminance * cloudscapeSample.transmittance;

			const float fogTransmittance = EvaluateHeightBasedTransmittanceForSegment(CONSTS->heightFog, worldLocation, reservoirHitLocation);
			luminance *= fogTransmittance;

			luminance += SampleCloudscapeInScattering(worldLocation, rayDirection);

			const GeneratedRayPDF rayPdf = LoadGeneratedRayPDF(CONSTS->rayPdfs, traceCommandIndex);

			SRReservoir reservoir = SRReservoir::Create(reservoirHitLocation, 0.f, luminance, rayPdf.pdf);

			reservoir.AddFlag(SR_RESERVOIR_FLAGS_MISS);
			reservoir.AddFlag(SR_RESERVOIR_FLAGS_VALIDATED);

			if(rayPdf.isSpecularTrace)
			{
				reservoir.AddFlag(SR_RESERVOIR_FLAGS_SPECULAR_TRACE);
			}

			reservoir.luminance = LuminanceToExposedLuminance(reservoir.luminance);

			WriteReservoirToScreenBuffer(CONSTS->reservoirsBuffer, CONSTS->reservoirsResolution, reservoir, traceCommand);
		}
	}
}
