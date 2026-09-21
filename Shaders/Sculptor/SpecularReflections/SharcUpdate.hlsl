#include "SculptorShader.hlsli"

#define SHARC_UPDATE 1
#define RT_MATERIAL_TRACING

[[shader_params(RenderSceneConstants, SCENE)]]
[[shader_params(GPURenderView, VIEW)]]
[[shader_params(SharcUpdateParams, PARAMS_SHARC_UPDATE)]]
[[shader_params(GlobalLightsParams, PARAMS_GLOBAL_LIGHTS)]]
[[shader_params(CloudscapeProbesParams, PARAMS_CLOUDSCAPE_PROBES)]]

#include "SpecularReflections/RTGITracing.hlsli"

#include "SpecularReflections/SculptorSharc.hlsli"

#include "Atmosphere/Atmosphere.hlsli"
#include "Atmosphere/VolumetricClouds/Cloudscape.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"

#include "Lights/Lighting.hlsli"

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"
#include "Utils/ScreenSpaceTracer.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"


float3 QueryLuminanceInPreviousCache(in float3 rayDirection, in float3 hitLocation, in const RayHitResult hitResult)
{
	SharcDef sharcDef;
	sharcDef.cameraPosition = VIEW->prevFrameSceneView.viewLocation;
	sharcDef.capacity       = 1u << 22;
	sharcDef.hashEntries    = PARAMS_SHARC_UPDATE->hashEntriesPrev.GetResource();
	sharcDef.voxelData      = PARAMS_SHARC_UPDATE->voxelDataPrev.GetResource();
	sharcDef.exposure       = VIEW->viewExposure->exposureLastFrame;

	const SharcParameters sharcParams = CreateSharcParameters(sharcDef);

	float3 luminance = 0.f;

	float3 diffuseColor;
	float3 specularColor;
	ComputeSurfaceColor(hitResult.baseColor.rgb, hitResult.metallic, OUT diffuseColor, OUT specularColor);

	SharcHitData hitData;
	hitData.positionWorld        = hitLocation;
	hitData.normalWorld          = hitResult.normal;

#if SHARC_DEMODULATE_MATERIALS
	const float NdotV = dot(-rayDirection, hitResult.normal);
	const float3 materialDemodulation = ComputeMaterialDemodulation(PARAMS_GLOBAL_LIGHTS->brdfIntegrationLUT, BindlessSamplers::LinearClampEdge(), diffuseColor, specularColor, NdotV, hitResult.roughness);
	hitData.materialDemodulation = materialDemodulation;
#endif // SHARC_DEMODULATE_MATERIALS

#if SHARC_SEPARATE_EMISSIVE
	hitData.emissive = hitResult.emissive;
#endif // SHARC_SEPARATE_EMISSIVE

	const bool success = SharcGetCachedRadiance(sharcParams, hitData, OUT luminance, false);

	return luminance;
}


float3 ShadeMissRay(in float3 rayOrigin, in float3 rayDirection)
{
	const float3 locationInAtmoshpere = GetLocationInAtmosphere(*PARAMS_SHARC_UPDATE->atmosphereParams, rayOrigin);
	float3 luminance = GetLuminanceFromSkyViewLUT(*PARAMS_SHARC_UPDATE->atmosphereParams, PARAMS_SHARC_UPDATE->skyViewLUT, BindlessSamplers::LinearClampEdge(), locationInAtmoshpere, rayDirection);

	const CloudscapeSample cloudscapeSample = SampleHighResCloudscape(rayDirection);
	luminance = cloudscapeSample.inScattering + luminance * cloudscapeSample.transmittance;

	const float fogTransmittance = EvaluateHeightBasedTransmittanceForSegment(PARAMS_GLOBAL_LIGHTS->heightFog, rayOrigin, rayOrigin + rayDirection * 1000.f);
	luminance *= fogTransmittance;

	return luminance;
}


float3 ShadeHitRay(in float3 hitLocation, in float3 rayDirection, in const RayHitResult hitResult, in bool isLastBounce)
{
	if(isLastBounce)
	{
		return QueryLuminanceInPreviousCache(rayDirection, hitLocation, hitResult);
	}

	ShadedSurface surface;
	surface.location       = hitLocation;
	surface.shadingNormal  = hitResult.normal;
	surface.geometryNormal = hitResult.normal;
	surface.roughness      = hitResult.roughness;
	ComputeSurfaceColor(hitResult.baseColor, hitResult.metallic, surface.diffuseColor, surface.specularColor);

	float3 luminance = CalcReflectedLuminance_Direct(surface, -rayDirection);

#if !SHARC_SEPARATE_EMISSIVE
	luminance += hitResult.emissive;
#endif

	return luminance;
}


[shader("raygeneration")]
void SharcUpdateRTG()
{
	const uint2 coords = DispatchRaysIndex().xy * 5u + uint2(PARAMS_SHARC_UPDATE->sharcConstants.frameIdx % 5u, (PARAMS_SHARC_UPDATE->sharcConstants.frameIdx % 25u) / 5u);

	RngState rng = RngState::Create(coords, PARAMS_SHARC_UPDATE->sharcConstants.seed);

	const GBufferInterface gbuffer = GBufferInterface(PARAMS_SHARC_UPDATE->sharcConstants.gpuGBuffer);

	const SurfaceInfo mainSurface = gbuffer.GetSurfaceInfo(coords);

	float3 worldLocation     = mainSurface.location;
	float3 normal            = mainSurface.normal;
	float  roughness         = mainSurface.roughness;
	float4 baseColorMetallic = mainSurface.baseColorMetallic;
	float3 fromDir           = normalize(VIEW->sceneView.viewLocation - worldLocation);

	float3 throughput = 1.f;
	float3 luminance = 0.f;

	SharcState sharcState;
	SharcInit(INOUT sharcState);

	SharcDef sharcDef;
	sharcDef.cameraPosition = VIEW->sceneView.viewLocation;
	sharcDef.capacity       = PARAMS_SHARC_UPDATE->sharcConstants.sharcCapacity;
	sharcDef.hashEntries    = PARAMS_SHARC_UPDATE->hashEntries.GetResource();
	sharcDef.voxelData      = PARAMS_SHARC_UPDATE->voxelData.GetResource();
	sharcDef.voxelDataPrev  = PARAMS_SHARC_UPDATE->voxelDataPrev.GetResource();
	sharcDef.exposure       = VIEW->viewExposure->exposure;

	SharcParameters sharcParams = CreateSharcParameters(sharcDef);

	float3 diffuseColor;
	float3 specularColor;

	{
		const bool isLastBounce = false;

		ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT specularColor);
		diffuseColor  = min(diffuseColor, 0.9f);
		specularColor = min(specularColor, 0.9f);

		// Primary ray
		RayHitResult hitRes;
		hitRes.normal      = normal;
		hitRes.roughness   = roughness;
		hitRes.baseColor   = baseColorMetallic.xyz;
		hitRes.metallic    = baseColorMetallic.w;
		hitRes.emissive    = mainSurface.emissive;
		hitRes.hitType     = RTGBUFFER_HIT_TYPE_VALID_HIT;
		hitRes.hitDistance = distance(VIEW->sceneView.viewLocation, worldLocation);
		const float3 Li = ShadeHitRay(worldLocation, normalize(worldLocation - VIEW->sceneView.viewLocation), hitRes, isLastBounce);

		SharcHitData hitData;
		hitData.positionWorld = worldLocation;
		hitData.normalWorld   = normal;

#if SHARC_DEMODULATE_MATERIALS
		const float NdotV = saturate(dot(normal, fromDir));
		const float3 materialDemodulation = ComputeMaterialDemodulation(PARAMS_GLOBAL_LIGHTS->brdfIntegrationLUT, BindlessSamplers::LinearClampEdge(), diffuseColor, specularColor, NdotV, roughness);
		hitData.materialDemodulation = materialDemodulation;
#endif // SHARC_DEMODULATE_MATERIALS

#if SHARC_SEPARATE_EMISSIVE
		hitData.emissive      = hitRes.emissive;
#endif // SHARC_SEPARATE_EMISSIVE

		if (!SharcUpdateHit(sharcParams, sharcState, hitData, Li, rng.Next()))
		{
			return;
		}
	}

	const uint bouncesNum = 4u;

	uint hitsNum = 0u;

	[unroll]
	for(uint it = 0u; it < bouncesNum; ++it)
	{
		hitsNum++;
		const RayDirectionInfo rayInfo = GenerateReflectionRayDir(diffuseColor, specularColor, normal, roughness, fromDir, rng);

		RayHitResult hitRes;
		bool isScreenSpaceHit = false;
		[branch]
		if (it == 0u)
		{
			const float traceDist = PARAMS_SHARC_UPDATE->sharcConstants.ssrTraceLength;
			const SSTraceResultExtended ssResult = TraceScreenSpaceRay(PARAMS_SHARC_UPDATE->sharcConstants.ssrTracer, VIEW->sceneView, mainSurface.uv, mainSurface.depth, rayInfo.direction, traceDist, rng.Next());

			if (ssResult.isHit)
			{
				const SurfaceInfo hitSurfaceInfo = gbuffer.GetSurfaceInfo(ssResult.hitUV);
				hitRes.normal      = hitSurfaceInfo.normal;
				hitRes.roughness   = hitSurfaceInfo.roughness;
				hitRes.baseColor   = hitSurfaceInfo.baseColorMetallic.rgb;
				hitRes.metallic    = hitSurfaceInfo.baseColorMetallic.w;
				hitRes.emissive    = hitSurfaceInfo.emissive;
				hitRes.hitType     = RTGBUFFER_HIT_TYPE_VALID_HIT;
				hitRes.hitDistance = traceDist * ssResult.hitT;

				isScreenSpaceHit = true;
			}

			worldLocation += rayInfo.direction * ssResult.unoccludedDistance * 0.9f;
		}

		if (!isScreenSpaceHit)
		{
			hitRes = RTGITraceRay(worldLocation, rayInfo.direction);
		}

		if (hitRes.hitType == RTGBUFFER_HIT_TYPE_BACKFACE)
		{
			luminance = 0.f;
			break;
		}

		hitRes.roughness = max(hitRes.roughness, 0.4f);

		const float nDotL = dot(normal, rayInfo.direction);
		const RTBRDF brdf = RT_EvaluateBRDF(normal, fromDir, rayInfo.direction, roughness, specularColor, diffuseColor);

		const float3 evaluatedBrdf = brdf.diffuse + brdf.specular;

		const float3 segmentThroughput = evaluatedBrdf * nDotL / rayInfo.pdf;

		SharcSetThroughput(sharcState, segmentThroughput);

		if (hitRes.hitType == RTGBUFFER_HIT_TYPE_VALID_HIT)
		{
			const bool isLastBounce = (it == (bouncesNum - 1u));

			const float3 hitLocation = worldLocation + rayInfo.direction * hitRes.hitDistance;

			const float3 Li = ShadeHitRay(hitLocation, rayInfo.direction, hitRes, isLastBounce);

			worldLocation = worldLocation + rayInfo.direction * hitRes.hitDistance;
			normal        = hitRes.normal;
			roughness     = max(hitRes.roughness, 0.4f);
			fromDir       = -rayInfo.direction;

			baseColorMetallic = float4(hitRes.baseColor, hitRes.metallic);
			ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT specularColor);
			diffuseColor = min(diffuseColor, 0.9f);
			specularColor = min(specularColor, 0.9f);

			const uint  gridLevel = HashGridGetLevel(hitLocation, sharcParams.gridParameters);
			const float voxelSize = HashGridGetVoxelSize(gridLevel, sharcParams.gridParameters);

			if (hitRes.hitDistance > voxelSize)
			{
				SharcHitData hitData;
				hitData.positionWorld        = worldLocation;
				hitData.normalWorld          = normal;

#if SHARC_DEMODULATE_MATERIALS
				const float NdotV = saturate(dot(normal, fromDir));
				const float3 materialDemodulation = ComputeMaterialDemodulation(PARAMS_GLOBAL_LIGHTS->brdfIntegrationLUT, BindlessSamplers::LinearClampEdge(), diffuseColor, specularColor, NdotV, roughness);
				hitData.materialDemodulation = materialDemodulation;
#endif // SHARC_DEMODULATE_MATERIALS

#if SHARC_SEPARATE_EMISSIVE
				hitData.emissive = hitRes.emissive;
#endif // SHARC_SEPARATE_EMISSIVE

				if (!SharcUpdateHit(sharcParams, sharcState, hitData, Li, rng.Next()))
				{
					break;
				}
			}

			// Apply bias
			worldLocation += fromDir * 0.01f;
		}
		else if(hitRes.hitType == RTGBUFFER_HIT_TYPE_NO_HIT)
		{
			const float3 Li = ShadeMissRay(worldLocation, rayInfo.direction);

			SharcUpdateMiss(sharcParams, sharcState, Li);
			break;
		}
		else
		{
			break;
		}
	}
}
