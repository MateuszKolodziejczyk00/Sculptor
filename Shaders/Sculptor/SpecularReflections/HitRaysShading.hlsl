#include "SculptorShader.hlsli"

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(GPURenderView, VIEW)]]

[[shader_params(RTShadingConstants, CONSTS)]]
[[shader_params(GlobalLightsParams, PARAMS_GLOBAL_LIGHTS)]]

#ifndef USE_DDGI
#error "USE_DDGI must be defined"
#endif // USE_DDGI

#if USE_DDGI
[[shader_params(DDGIGPUScene, PARAMS_D_D_G_I_SCENE)]]
#else
[[shader_params(SharcCacheParams, PARAMS_SHARC_CACHE)]]
#endif // USE_DDGI


#include "RayTracing/RayTracingHelpers.hlsli"


#if !USE_DDGI
#include "SpecularReflections/SculptorSharcQuery.hlsli"
#endif // !USE_DDGI

#include "Utils/SceneViewUtils.hlsli"
#include "Lights/Lighting.hlsli"
#include "SpecularReflections/SRReservoir.hlsli"
#include "SpecularReflections/RTGBuffer.hlsli"
#include "SpecularReflections/RTReflectionsShadingCommon.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


[shader("raygeneration")]
void HitRaysShadingRTG()
{
	const uint hitIdx = DispatchRaysIndex().x;

	if(hitIdx >= CONSTS->tracesNum[0].hitRaysNum)
	{
		return;
	}

	const uint traceCommandIndex = CONSTS->sortedTraces[hitIdx];

	const EncodedRayTraceCommand encodedTraceCommand = CONSTS->traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 pixel = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = CONSTS->depthTexture.Load(uint3(pixel, 0));
	if(depth == 0.f)
	{
		return;
	}

	const float2 uv = (pixel + 0.5f) * CONSTS->invResolution;
	const float3 ndc = float3(uv * 2.f - 1.f, depth);

	const float3 worldLocation = NDCToWorldSpace(ndc, VIEW->sceneView);

	const RayHitResult hitResult = UnpackRTGBuffer(CONSTS->hitMaterialInfos[traceCommandIndex]);

	if(hitResult.hitType == RTGBUFFER_HIT_TYPE_VALID_HIT)
	{
		const uint encodedRayDirection = CONSTS->rayDirections[traceCommandIndex];
		const float3 rayDirection = OctahedronDecodeNormal(UnpackHalf2x16Norm(encodedRayDirection));

		const float3 hitLocation = worldLocation + rayDirection * hitResult.hitDistance;

		SurfaceGeometry surfaceGeometry;
		surfaceGeometry.location  = hitLocation;
		surfaceGeometry.geoNormal = hitResult.normal;

		const ShadedSurface surface = hitResult.AsShadedSurface();

		const float3 primaryHitToView = normalize(VIEW->sceneView.viewLocation - worldLocation);

		float3 luminance;
#if USE_DDGI
		luminance = CalcReflectedLuminance(surface, surfaceGeometry, -rayDirection, DDGISecondaryBounceSampleContext::Create(worldLocation, primaryHitToView), 1.f);
#else
        const float3 tangent = abs(dot(hitResult.normal, UP_VECTOR)) > 0.9f ? cross(hitResult.normal, RIGHT_VECTOR) : cross(hitResult.normal, UP_VECTOR);
        const float3 bitangent = cross(hitResult.normal, tangent);

		const HashGridParameters gridParams = CreateHashGridParameters(VIEW->sceneView.viewLocation);

		const uint  gridLevel = HashGridGetLevel(hitLocation, gridParams);
		const float voxelSize = HashGridGetVoxelSize(gridLevel, gridParams);

		RngState rng = RngState::Create(pixel, CONSTS->frameIdx);

		const float2 normalizedOffset = float2(rng.Next(), rng.Next()) * 2.f - 1.f;

		const float3 sampledLocation = hitLocation + voxelSize * normalizedOffset.x * tangent + voxelSize * normalizedOffset.y * bitangent;

		const float NdotV = dot(-rayDirection, hitResult.normal);

		SharcQuery query;
		query.location = sampledLocation;
		query.normal   = hitResult.normal;
#if SHARC_MATERIAL_DEMODULATION
		query.materialDemodulation = ComputeMaterialDemodulation(PARAMS_GLOBAL_LIGHTS->brdfIntegrationLUT, BindlessSamplers::LinearClampEdge(), surface.albedo, surface.f0, NdotV, surface.roughness);
#endif // SHARC_MATERIAL_DEMODULATION

		// We don't want to include emissive in the secondary bounce, because it will be handled by stochastic DI
#if SHARC_SEPARATE_EMISSIVE
		query.emissive = 0.f;
#endif // SHARC_SEPARATE_EMISSIVE
		if (!QueryCachedLuminance(VIEW->sceneView.viewLocation, VIEW->viewExposure->exposure, query, OUT luminance))
		{
			luminance = 0.f;
		}
#endif // USE_DDGI

		const GeneratedRayPDF rayPdf = LoadGeneratedRayPDF(CONSTS->rayPdfs, traceCommandIndex);

		SRReservoir reservoir = SRReservoir::Create(hitLocation, hitResult.normal, luminance, rayPdf.pdf);

		reservoir.luminance = LuminanceToExposedLuminance(reservoir.luminance);

		reservoir.AddFlag(SR_RESERVOIR_FLAGS_VALIDATED);

		if(rayPdf.isSpecularTrace)
		{
			reservoir.AddFlag(SR_RESERVOIR_FLAGS_SPECULAR_TRACE);
		}

		WriteReservoirToScreenBuffer(CONSTS->reservoirsBuffer, CONSTS->reservoirsResolution, reservoir, traceCommand);
	}
}


[shader("miss")]
void ShadowRayRTM(inout ShadowRayPayload payload)
{
	payload.isShadowed = false;
}
