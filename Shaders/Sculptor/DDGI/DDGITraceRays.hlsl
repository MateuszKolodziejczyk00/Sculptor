#include "SculptorShader.hlsli"

#define RT_MATERIAL_TRACING

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(GlobalLightsParams, PARAMS_GLOBAL_LIGHTS)]]
[[shader_params(DDGIGPUScene, PARAMS_DDGI_SCENE)]]
[[shader_params(CloudscapeProbesParams, PARAMS_CLOUDSCAPE_PROBES)]]
[[shader_params(DDGITraceRaysConsts, CONSTS)]]


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
	const uint3 probeCoords = ComputeUpdatedProbeCoords(dispatchIdx.x, CONSTS->relitParams->probesToUpdateCoords, CONSTS->relitParams->probesToUpdateCount);

	float3 rayDirection = GetProbeRayDirection(rayIdx, CONSTS->relitParams->raysNumPerProbe);

	const float3 probeWorldLocation = GetProbeWorldLocation(*CONSTS->volumeParams, probeCoords);

	RayDesc rayDesc;
	rayDesc.TMin        = CONSTS->relitParams->probeRaysMinT;
	rayDesc.TMax        = CONSTS->relitParams->probeRaysMaxT;
	rayDesc.Origin      = probeWorldLocation;
	rayDesc.Direction   = rayDirection;

	rayDesc.TMin        = 0.001f;
	rayDesc.TMax        = 0.002f;
	rayDesc.Origin      = 9999.f;
	rayDesc.Direction   = 1.f;

	RayPayloadData traceResult;
	traceResult.visibility.isValidHit = false;
	traceResult.visibility.isMiss = RTScene().VisibilityTest(rayDesc);

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
		const float3 probeAtmosphereLocation = GetLocationInAtmosphere(*CONSTS->atmosphereParams, probeWorldLocation);
		luminance = GetLuminanceFromSkyViewLUT(*CONSTS->atmosphereParams, CONSTS->skyViewLUT, BindlessSamplers::LinearClampEdge(), probeAtmosphereLocation, rayDirection);

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
		const float fogTransmittance = EvaluateHeightBasedTransmittanceForSegment(PARAMS_GLOBAL_LIGHTS->heightFog, probeWorldLocation, traceEndLocation);
		luminance *= fogTransmittance;
	}

	CONSTS->traceRaysResultTexture[dispatchIdx] = float4(luminance, distanceTraveled);
}
