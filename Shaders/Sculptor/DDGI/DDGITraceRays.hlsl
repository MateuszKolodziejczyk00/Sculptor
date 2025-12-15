#include "SculptorShader.hlsli"

#define RT_MATERIAL_TRACING

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(GlobalLightsDS)]]
[[descriptor_set(ShadowMapsDS)]]
[[descriptor_set(DDGISceneDS)]]
[[descriptor_set(CloudscapeProbesDS)]]
[[descriptor_set(DDGITraceRaysDS)]]


#include "RayTracing/RayTracingHelpers.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Atmosphere/VolumetricClouds/Cloudscape.hlsli"
#include "DDGI/DDGITypes.hlsli"
#include "Lights/Lighting.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"


[shader("raygeneration")]
void DDGIProbeRaysRTG()
{
	const uint2 dispatchIdx = DispatchRaysIndex().xy;
	
	const uint rayIdx = dispatchIdx.y;
	const uint3 probeCoords = ComputeUpdatedProbeCoords(dispatchIdx.x, u_relitParams.probesToUpdateCoords, u_relitParams.probesToUpdateCount);

	float3 rayDirection = GetProbeRayDirection(rayIdx, u_relitParams.raysNumPerProbe);

	const float3 probeWorldLocation = GetProbeWorldLocation(u_volumeParams, probeCoords);

	RayDesc rayDesc;
	rayDesc.TMin        = u_relitParams.probeRaysMinT;
	rayDesc.TMax        = u_relitParams.probeRaysMaxT;
	rayDesc.Origin      = probeWorldLocation;
	rayDesc.Direction   = rayDirection;

	const RayPayloadData traceResult = RTScene().TraceMaterialRay(rayDesc);

	float3 luminance = 0.f;

	float3 traceEndLocation = 0.f;
	float distanceTraveled = 0.f;

	if(traceResult.visibility.isValidHit)
	{
		float4 baseColorMetallic = UnpackFloat4x8(traceResult.material.baseColorMetallic);

		const float3 hitNormal = traceResult.material.normal;

		// Introduce additional bias as using just distance can have too low precision (which results in artifacts f.e. when using shadow maps)
		const float locationBias = 0.03f;
		const float3 worldLocation = probeWorldLocation + rayDirection * traceResult.material.distance + hitNormal * locationBias;

		ShadedSurface surface;
		surface.location        = worldLocation;
		surface.shadingNormal   = hitNormal;
		surface.geometryNormal  = hitNormal;
		surface.roughness       = traceResult.material.roughness;
		ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, surface.diffuseColor, surface.specularColor);

		const float recursionMultiplier = 0.9f;
		
		luminance = CalcReflectedLuminance(surface, -rayDirection, DDGISampleContext::Create(), recursionMultiplier);

		luminance += traceResult.material.emissive;

		traceEndLocation = worldLocation;
		distanceTraveled = traceResult.material.distance;
	}
	else if (traceResult.visibility.isMiss)
	{
		const float3 probeAtmosphereLocation = GetLocationInAtmosphere(u_atmosphereParams, probeWorldLocation);
		luminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, probeAtmosphereLocation, rayDirection);

		const CloudscapeSample cloudscapeSample = SampleCloudscape(probeWorldLocation, rayDirection);
		luminance = cloudscapeSample.inScattering + luminance * cloudscapeSample.transmittance;

		distanceTraveled = 1500.f;
		traceEndLocation = probeWorldLocation + rayDirection * distanceTraveled;
	}
	else // backface
	{
		traceEndLocation = probeWorldLocation + rayDirection * traceResult.material.distance;
		distanceTraveled = -traceResult.material.distance;
	}

	const bool isBackface = distanceTraveled > 0.f;

	if (!isBackface)
	{
		const float fogTransmittance = EvaluateHeightBasedTransmittanceForSegment(u_lightsParams.heightFog, probeWorldLocation, traceEndLocation);
		luminance *= fogTransmittance;
	}

	u_traceRaysResultTexture[dispatchIdx] = float4(luminance, distanceTraveled);
}
