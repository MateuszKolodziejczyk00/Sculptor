#include "SculptorShader.hlsli"

[[shader_params(RTShadingConstants, PARAMS_R_T_SHADING)]]
[[shader_params(GPURenderView, VIEW)]]
[[shader_params(CloudscapeProbesParams, PARAMS_CLOUDSCAPE_PROBES)]]

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

	const uint missRaysNum = PARAMS_R_T_SHADING->tracesNum[0].missRaysNum;

	if(missIdx >= missRaysNum)
	{
		return;
	}

	const uint missRaysOffset = PARAMS_R_T_SHADING->rayCommandsBufferSize - missRaysNum;
	const uint rayIdx = missRaysOffset + missIdx;

	const uint traceCommandIndex = PARAMS_R_T_SHADING->sortedTraces[rayIdx];

	const EncodedRayTraceCommand encodedTraceCommand = PARAMS_R_T_SHADING->traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 pixel = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = PARAMS_R_T_SHADING->depthTexture.Load(uint3(pixel, 0));
	if(depth > 0.f)
	{
		const float2 uv = (pixel + 0.5f) * PARAMS_R_T_SHADING->invResolution;
		const float3 ndc = float3(uv * 2.f - 1.f, depth);

		const float3 worldLocation = NDCToWorldSpace(ndc, VIEW->sceneView);
		
		const RayHitResult hitResult = UnpackRTGBuffer(PARAMS_R_T_SHADING->hitMaterialInfos[traceCommandIndex]);

		if(hitResult.hitType == RTGBUFFER_HIT_TYPE_NO_HIT)
		{
			const uint encodedRayDirection = PARAMS_R_T_SHADING->rayDirections[traceCommandIndex];
			const float3 rayDirection = OctahedronDecodeNormal(UnpackHalf2x16Norm(encodedRayDirection));

			const float3 reservoirHitLocation = worldLocation + rayDirection * 2000.f;

			const float3 locationInAtmoshpere = GetLocationInAtmosphere(*PARAMS_R_T_SHADING->atmosphereParams, worldLocation);
			float3 luminance = GetLuminanceFromSkyViewLUT(*PARAMS_R_T_SHADING->atmosphereParams, PARAMS_R_T_SHADING->skyViewLUT, BindlessSamplers::LinearClampEdge(), locationInAtmoshpere, rayDirection);

			//const CloudscapeSample cloudscapeSample = SampleHighResCloudscape(rayDirection);
			const CloudscapeSample cloudscapeSample = SampleCloudscape(worldLocation, rayDirection);
			luminance = cloudscapeSample.inScattering + luminance * cloudscapeSample.transmittance;

			const float fogTransmittance = EvaluateHeightBasedTransmittanceForSegment(PARAMS_R_T_SHADING->heightFog, worldLocation, reservoirHitLocation);
			luminance *= fogTransmittance;

			const GeneratedRayPDF rayPdf = LoadGeneratedRayPDF(PARAMS_R_T_SHADING->rayPdfs, traceCommandIndex);

			SRReservoir reservoir = SRReservoir::Create(reservoirHitLocation, 0.f, luminance, rayPdf.pdf);

			reservoir.AddFlag(SR_RESERVOIR_FLAGS_MISS);
			reservoir.AddFlag(SR_RESERVOIR_FLAGS_VALIDATED);

			if(rayPdf.isSpecularTrace)
			{
				reservoir.AddFlag(SR_RESERVOIR_FLAGS_SPECULAR_TRACE);
			}

			reservoir.luminance = LuminanceToExposedLuminance(reservoir.luminance);

			WriteReservoirToScreenBuffer(PARAMS_R_T_SHADING->reservoirsBuffer, PARAMS_R_T_SHADING->reservoirsResolution, reservoir, traceCommand);
		}
	}
}
