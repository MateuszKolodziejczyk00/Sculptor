#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 2)]]

[[descriptor_set(RTShadingDS, 3)]]
[[descriptor_set(GlobalLightsDS, 4)]]
[[descriptor_set(ShadowMapsDS, 5)]]

#ifndef USE_DDGI
#error "USE_DDGI must be defined"
#endif // USE_DDGI

#if USE_DDGI
[[descriptor_set(DDGISceneDS, 6)]]
#else
[[descriptor_set(SharcCacheDS, 6)]]
#endif // USE_DDGI


#include "Utils/RTVisibilityCommon.hlsli"

#define SPT_LIGHTING_SHADOW_RAY_MISS_SHADER_IDX 0


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
		if (!QueryCachedLuminance(u_sceneView.viewLocation, u_viewExposure.exposure, hitLocation, hitResult.normal, OUT luminance))
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
