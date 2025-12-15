#include "SculptorShader.hlsli"

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]

[[descriptor_set(RTShadingDS)]]
[[descriptor_set(GlobalLightsDS)]]
[[descriptor_set(ShadowMapsDS)]]

#ifndef USE_DDGI
#error "USE_DDGI must be defined"
#endif // USE_DDGI

#if USE_DDGI
[[descriptor_set(DDGISceneDS)]]
#else
[[descriptor_set(SharcCacheDS)]]
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
#include "SpecularReflections/RTGICommon.hlsli"

#include "Utils/VariableRate/Tracing/RayTraceCommand.hlsli"
#include "Utils/VariableRate/VariableRate.hlsli"


[shader("raygeneration")]
void HitRaysShadingRTG()
{
	const uint hitIdx = DispatchRaysIndex().x;

	if(hitIdx >= u_tracesNum[0].hitRaysNum)
	{
		return;
	}

	const uint traceCommandIndex = u_sortedTraces[hitIdx];

	const EncodedRayTraceCommand encodedTraceCommand = u_traceCommands[traceCommandIndex];
	const RayTraceCommand traceCommand = DecodeTraceCommand(encodedTraceCommand);

	const uint2 pixel = traceCommand.blockCoords + traceCommand.localOffset;

	const float depth = u_depthTexture.Load(uint3(pixel, 0));
	if(depth == 0.f)
	{
		return;
	}

	const float2 uv = (pixel + 0.5f) * u_constants.invResolution;
	const float3 ndc = float3(uv * 2.f - 1.f, depth);

	const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

	const RayHitResult hitResult = UnpackRTGBuffer(u_hitMaterialInfos[traceCommandIndex]);

	if(hitResult.hitType == RTGBUFFER_HIT_TYPE_VALID_HIT)
	{
		const uint encodedRayDirection = u_rayDirections[traceCommandIndex];
		const float3 rayDirection = OctahedronDecodeNormal(UnpackHalf2x16Norm(encodedRayDirection));

		const float3 hitLocation = worldLocation + rayDirection * hitResult.hitDistance;

		const float minSecondaryRoughness = 0.6f;

		ShadedSurface surface;
		surface.location       = hitLocation;
		surface.shadingNormal  = hitResult.normal;
		surface.geometryNormal = hitResult.normal;
		surface.roughness      = max(hitResult.roughness, minSecondaryRoughness);
		ComputeSurfaceColor(hitResult.baseColor, hitResult.metallic, surface.diffuseColor, surface.specularColor);

		const float3 primaryHitToView = normalize(u_sceneView.viewLocation - worldLocation);

		float3 luminance;
#if USE_DDGI
		luminance = CalcReflectedLuminance(surface, -rayDirection, DDGISecondaryBounceSampleContext::Create(worldLocation, primaryHitToView), 1.f);
#else
        const float3 tangent = abs(dot(hitResult.normal, UP_VECTOR)) > 0.9f ? cross(hitResult.normal, RIGHT_VECTOR) : cross(hitResult.normal, UP_VECTOR);
        const float3 bitangent = cross(hitResult.normal, tangent);

		const HashGridParameters gridParams = CreateHashGridParameters(u_sceneView.viewLocation);

		const uint  gridLevel = HashGridGetLevel(hitLocation, gridParams);
		const float voxelSize = HashGridGetVoxelSize(gridLevel, gridParams);

		RngState rng = RngState::Create(pixel, u_constants.frameIdx);

		const float2 normalizedOffset = float2(rng.Next(), rng.Next()) * 2.f - 1.f;

		const float3 sampledLocation = hitLocation + voxelSize * normalizedOffset.x * tangent + voxelSize * normalizedOffset.y * bitangent;

		const float NdotV = dot(-rayDirection, hitResult.normal);
		const float3 materialDemodulation = ComputeMaterialDemodulation(u_brdfIntegrationLUT, u_brdfIntegrationLUTSampler, surface.diffuseColor, surface.specularColor, NdotV, surface.roughness);
		if (!QueryCachedLuminance(u_sceneView.viewLocation, u_viewExposure.exposure, materialDemodulation, sampledLocation, hitResult.normal, OUT luminance))
		{
			luminance = 0.f;
		}
#endif // USE_DDGI

		luminance += hitResult.emissive;

		const GeneratedRayPDF rayPdf = LoadGeneratedRayPDF(u_rayPdfs, traceCommandIndex);

		SRReservoir reservoir = SRReservoir::Create(hitLocation, hitResult.normal, luminance, rayPdf.pdf);

		reservoir.luminance = LuminanceToExposedLuminance(reservoir.luminance);

		reservoir.AddFlag(SR_RESERVOIR_FLAGS_VALIDATED);

		if(rayPdf.isSpecularTrace)
		{
			reservoir.AddFlag(SR_RESERVOIR_FLAGS_SPECULAR_TRACE);
		}

		WriteReservoirToScreenBuffer(u_reservoirsBuffer, u_constants.reservoirsResolution, reservoir, traceCommand);
	}
}


[shader("miss")]
void ShadowRayRTM(inout ShadowRayPayload payload)
{
	payload.isShadowed = false;
}
